#include <iostream>
#include <dirent.h>  // directory stuff
#include <memory>    // for unique_ptr
#include <vector>    // for vector
#include <string>    // for string
#include <iomanip>   // for std::setw
#include <fstream>   // for file operations
#include <algorithm> // for all_of
#include <sstream>   // for stringstream
#include <pwd.h>     // for getpwuid
#include <chrono>    // for std::chrono
#include <thread>    // for std::this_thread
#include <csignal>   // for signal handling
#include <unistd.h>  // to get the number of clock ticks per second
#include <map>
#include <functional>
using namespace std;

class Process
{
private:
    int pid;
    string name;
    int priority;
    double memoryUsage;
    string status;
    string owner;
    int ppid;
    unsigned long utimeCurrent;
    unsigned long stimeCurrent;
    double cpuUsage;
    static long clk_tck;

    string getUsernameFromUid(const string &uidStr)
    {
        try
        {
            uid_t uid = stoi(uidStr);          // convert the uid to an integer
            struct passwd *pw = getpwuid(uid); // get the user's name
            if (pw != nullptr)                 // if the user's name is not null
            {
                return string(pw->pw_name); // return the user's name
            }
        }
        catch (...)
        {
            cout << "Error converting UID to integer or getting username." << endl;
        }
        return "unknown";
    }

    double getTotalSystemMemory()
    {
        ifstream meminfo("/proc/meminfo"); // open the meminfo file
        string line;
        double totalMem = 0.0;
        if (getline(meminfo, line))
        {
            stringstream ss(line); // create a stringstream object
            string label;
            ss >> label >> totalMem; // read the label and total memory
        }
        return totalMem; // return the total memory
    }

    int getCpuCount()
    {
        ifstream cpuinfo("/proc/cpuinfo");
        string line;
        int cpuCount = 0;

        while (getline(cpuinfo, line))
        {
            if (line.find("processor") == 0)
            {
                cpuCount++;
            }
        }
        return cpuCount > 0 ? cpuCount : 1;
    }

    void fetchProcessDetails()
    {
        name = "N/A";
        priority = 0;
        memoryUsage = 0;
        status = '?';
        owner = "N/A";
        ppid = 0;
        cpuUsage = 0.0;

        long clk_tck = sysconf(_SC_CLK_TCK); // clock ticks per second

        string statFilePath = "/proc/" + to_string(pid) + "/stat"; // path for the stat file
        ifstream statFile(statFilePath);                           // open the stat file
        string statLine;                                           // line to hold the stat file content
        if (getline(statFile, statLine))
        {
            size_t firstParen = statLine.find('('); // we find the first and last parentheses as the processName is inside the parentheses
            size_t lastParen = statLine.rfind(')');

            if (firstParen != string::npos && lastParen != string::npos) // we check if the parentheses are found
            {
                // extract process name
                name = statLine.substr(firstParen + 1, lastParen - firstParen - 1);

                // Remove any spaces in the name
                name.erase(remove(name.begin(), name.end(), ' '), name.end());
            }
            {
                string restOfStat = statLine.substr(lastParen + 2); // take the rest of the information after the name
                stringstream ss(restOfStat);                        // create a stringstream object for the rest of the information
                vector<string> values;                              // store all the values in a vector
                string token;

                while (ss >> token)
                    values.push_back(token);

                if (values.size() >= 22)
                {
                    status = values[0][0];                       // 3rd field: process state
                    ppid = stoi(values[1]);                      // 4th field: parent PID
                    utimeCurrent = stoul(values[11]);            // 14th field
                    stimeCurrent = stoul(values[12]);            // 15th field
                    priority = stoi(values[16]);                 // 18th field
                    unsigned long starttime = stoul(values[19]); // 22nd field

                    // Read uptime from /proc/uptime
                    ifstream uptimeFile("/proc/uptime");
                    string uptimeLine;
                    if (getline(uptimeFile, uptimeLine))
                    {
                        stringstream uptimeStream(uptimeLine);
                        double uptimeSeconds = 0.0;
                        uptimeStream >> uptimeSeconds;

                        double totalCPUTime = utimeCurrent + stimeCurrent;              // calculate total CPU time used to know how much CPU time the process consumedd in user and kernel modes
                        double seconds = uptimeSeconds - (starttime / (double)clk_tck); // calculate the time since the process started
                        if (seconds > 0)
                            cpuUsage = 100.0 * ((totalCPUTime / clk_tck) / seconds);
                    }
                }
            }
        }
        statFile.close();

        string statusFilePath = "/proc/" + to_string(pid) + "/status"; // status path to calculate memory usage
        ifstream statusFile(statusFilePath);
        if (statusFile.is_open())
        {
            string line;
            while (getline(statusFile, line))
            {
                if (line.find("Uid:") == 0) // if we find UID at the very beginning of the line
                {
                    string uidStr = line.substr(5);
                    size_t tabPos = uidStr.find('\t');
                    if (tabPos != string::npos)
                    {
                        uidStr = uidStr.substr(0, tabPos);
                    }
                    owner = getUsernameFromUid(uidStr);
                }
                else if (line.find("VmRSS:") == 0)
                {
                    double memKB = stod(line.substr(7)); // convert the memory usage to double
                    double totalMem = getTotalSystemMemory();
                    if (totalMem > 0)
                    {
                        memoryUsage = (memKB / totalMem) * 100.0;
                    }
                }
            }
        }
        statusFile.close();
    }

public:
    Process(int p)
    {
        pid = p;
        fetchProcessDetails();
    }

    int getPID() const { return pid; }
    const string &getName() const { return name; }
    double getMemoryUsage() const { return memoryUsage; }
    const string &getOwner() const { return owner; }
    int getParentPID() const { return ppid; }
    const string &getStatus() const { return status; }
    int getPriority() const { return priority; }
    double getCPUUsage() const { return cpuUsage; }
};

bool running = true;           // flag for controlling auto-refresh
void signalHandler(int signum) // signal handler for Ctrl+C
{
    running = false;
}

void clearScreen() // clear the screen function
{
    cout << "\033[2J\033[1;1H"; // escape sequence to clear screen
}

bool isNumeric(const string &s)
{
    if (s.empty())
    {
        return false;
    }
    return std::all_of(s.begin(), s.end(), ::isdigit);
}

vector<unique_ptr<Process>> findProcesses()
{
    vector<unique_ptr<Process>> processesFound; // i use unique_ptr to manage memory automatically for automatic cleanup

    DIR *processesDirectory = opendir("/proc"); // open /proc directory
    if (!processesDirectory)
    {
        cout << "Error opening /proc directory" << endl;
    }

    struct dirent *entry; // directory entry (temporarily hold a pointer to a directory entry)
    while ((entry = readdir(processesDirectory)) != NULL)
    {                                // reads the next available entry
        if (entry->d_type == DT_DIR) // check if the entry is a directory
        {
            string dirName(entry->d_name); // convert to string
            if (isNumeric(dirName))
            {
                int pid = stoi(dirName); // convert to int
                if (pid > 0)
                {
                    processesFound.push_back(unique_ptr<Process>(new Process(pid))); // create a new Process object and add it to the vector
                }
            }
        }
    }
    closedir(processesDirectory); // close the directory
    return processesFound;        // return the vector of processes
}

// ANSI color codes
#define COLOR_RESET "\033[0m"
#define COLOR_HEADER "\033[1;36m"    // Bold cyan
#define COLOR_LABEL "\033[1;33m"     // Bold yellow
#define COLOR_VALUE "\033[0;37m"     // Light gray
#define COLOR_HIGHLIGHT "\033[1;32m" // Bold green

void displayProcesses(const vector<unique_ptr<Process>> &processList)
{
    cout << left;

    // Header section
    cout << COLOR_HEADER;
    cout << setw(8) << "PID"
         << setw(8) << "PPID"
         << setw(25) << "Name"
         << setw(12) << "Owner"
         << setw(12) << "Memory(%)"
         << setw(10) << "CPU(%)"
         << setw(8) << "Status"
         << setw(10) << "Priority"
         << COLOR_RESET << endl;

    cout << COLOR_LABEL << string(93, '-') << COLOR_RESET << endl;

    for (const auto &procPtr : processList)
    {
        if (!procPtr)
            continue;

        // You can highlight high memory or CPU processes
        bool isHighCPU = procPtr->getCPUUsage() > 10.0;
        bool isHighMem = procPtr->getMemoryUsage() > 5.0;

        cout << setw(8) << procPtr->getPID()
             << setw(8) << procPtr->getParentPID()
             << setw(25) << procPtr->getName().substr(0, 24)
             << setw(12) << procPtr->getOwner().substr(0, 11);

        // Highlight memory and CPU if high
        cout << fixed << setprecision(1);
        if (isHighMem)
            cout << COLOR_HIGHLIGHT;
        cout << setw(12) << procPtr->getMemoryUsage();
        cout << COLOR_RESET;

        if (isHighCPU)
            cout << COLOR_HIGHLIGHT;
        cout << setw(10) << procPtr->getCPUUsage();
        cout << COLOR_RESET;

        cout << setw(8) << procPtr->getStatus()
             << setw(10) << procPtr->getPriority()
             << endl;
    }

    cout << COLOR_LABEL << string(93, '-') << COLOR_RESET << endl;
    cout << COLOR_HEADER << "Total Processes: " << COLOR_VALUE << processList.size() << COLOR_RESET << endl
         << endl;
}

int main()
{
    signal(SIGINT, signalHandler); // Register signal handler for Ctrl+C

    cout << "--- Linux Process Lister ---" << endl;

    // get the initial list of processes
    cout << "Fetching process list..." << endl;
    vector<unique_ptr<Process>> currentProcesses = findProcesses();

    if (currentProcesses.empty())
    {
        cout << "No processes found or error reading /proc." << endl;
        return 1;
    }

    // display the list
    cout << "Displaying processes..." << endl;
    displayProcesses(currentProcesses);

    cout << "Enter command (e.g., 'refresh', 'auto', 'exit'):" << endl;
    cout << " - 'refresh': Update process list once" << endl;
    cout << " - 'auto [interval]': Auto-refresh every [interval] seconds (Ctrl+C to stop)" << endl;
    cout << " - 'sort': Sort the process list by memory/priority/pid/ppid/name/cpu" << endl;
    cout << " - 'exit': Quit the program" << endl;
    cout << " - 'filter': Filter processes by memory/priority/name/owner/cpu" << endl;
    cout << " - 'terminate': Terminate a process by PID" << endl;
    cout << " - 'group': Group processes by owner or parent PID" << endl;
    cout << " - 'expand owner [name]': Expand to show processes owned by [name]" << endl;
    cout << " - 'expand pid [pid]': Expand to show children of PID [pid]" << endl;
    cout << " - 'help': Show this help message" << endl;
    cout << "-------------------------------------" << endl;
    cout << "Type 'help' for available commands." << endl;
    cout << "-------------------------------------" << endl;

    string command;
    while (true)
    {
        cout << "LPM> ";
        if (!getline(cin, command))
        { // Handle potential input errors or EOF
            break;
        }
        if (command == "exit")
        {
            break;
        }
        else if (command == "refresh")
        {
            cout << "Refreshing process list..." << endl;
            currentProcesses = findProcesses(); // Get updated list
            displayProcesses(currentProcesses); // Display updated list
        }
        else if (command.substr(0, 4) == "auto")
        {
            int interval = 2; // Default interval in seconds

            if (command.length() > 5)
            {
                try
                {
                    interval = stoi(command.substr(5));
                    if (interval < 1)
                        interval = 1; // Minimum 1 second
                }
                catch (...)
                {
                    cout << "Invalid interval. Using default 2 seconds." << endl;
                }
            }

            cout << "Auto-refreshing every " << interval << " seconds. Press Ctrl+C to stop." << endl;
            running = true;

            while (running)
            {
                clearScreen();
                cout << "--- Auto-refreshing (every " << interval << "s) - Press Ctrl+C to stop ---" << endl;
                currentProcesses = findProcesses();
                displayProcesses(currentProcesses);

                // Sleep for the specified interval
                this_thread::sleep_for(chrono::seconds(interval));
            }

            cout << "Auto-refresh stopped." << endl;
        }
        else if (command == "help")
        {
            cout << "Available commands:\n";
            cout << "  refresh - Reload and display the process list.\n";
            cout << "  auto [seconds] - Automatically refresh the process list every [seconds] seconds.\n";
            cout << "  sort    - Sort the process list by memory/priority/pid/ppid/name/cpu.\n";
            cout << "  exit    - Quit the program.\n";
            cout << "  filter  - Filter processes by memory/priority/name/owner.\n";
            cout << "  terminate - Terminate a process by PID.\n";
            cout << "  group   - Group processes by owner or parent PID.\n";
            cout << "  expand owner [name] - Expand to show processes owned by [name].\n";
            cout << "  expand pid [pid] - Expand to show children of PID [pid].\n";
            cout << "  help    - Show this help message.\n";
            cout << "-------------------------------------" << endl;
            cout << "Type 'help' for available commands." << endl;
            cout << "-------------------------------------" << endl;
        }
        else if (command == "sort")
        {
            string sortBy;
            char ascOrDesc;
            string sortedBy;
            cout << "Sort by: (memory/priority/pid/ppid/name/cpu) " << endl;
            cin >> sortBy;
            cout << "Ascending or Descending? (a/d)" << endl;
            cin >> ascOrDesc;

            if (sortBy == "memory")
            {
                if (ascOrDesc == 'a')
                {
                    cout << "Sorting processes by memory usage in ascending order..." << endl;
                    sort(currentProcesses.begin(), currentProcesses.end(), [](const unique_ptr<Process> &a, const unique_ptr<Process> &b)
                         { return a->getMemoryUsage() < b->getMemoryUsage(); });
                    sortedBy = "Displayed processes in ascending order of memory usage.";
                }
                else if (ascOrDesc == 'd')
                {
                    cout << "Sorting processes by memory usage in descending order..." << endl;
                    sort(currentProcesses.begin(), currentProcesses.end(), [](const unique_ptr<Process> &a, const unique_ptr<Process> &b)
                         { return a->getMemoryUsage() > b->getMemoryUsage(); });
                    sortedBy = "Displayed processes in descending order of memory usage.";
                }
            }
            else if (sortBy == "priority")
            {
                if (ascOrDesc == 'a')
                {
                    cout << "Sorting processes by priority in ascending order..." << endl;
                    sort(currentProcesses.begin(), currentProcesses.end(), [](const unique_ptr<Process> &a, const unique_ptr<Process> &b)
                         { return a->getPriority() < b->getPriority(); });
                    sortedBy = "Displayed processes in ascending order of priority.";
                }
                else if (ascOrDesc == 'd')
                {
                    cout << "Sorting processes by priority in descending order..." << endl;
                    sort(currentProcesses.begin(), currentProcesses.end(), [](const unique_ptr<Process> &a, const unique_ptr<Process> &b)
                         { return a->getPriority() > b->getPriority(); });
                    sortedBy = "Displayed processes in descending order of priority.";
                }
            }
            else if (sortBy == "pid")
            {
                if (ascOrDesc == 'a')
                {
                    cout << "Sorting processes by PID in ascending order..." << endl;
                    sort(currentProcesses.begin(), currentProcesses.end(), [](const unique_ptr<Process> &a, const unique_ptr<Process> &b)
                         { return a->getPID() < b->getPID(); });
                    sortedBy = "Displayed processes in ascending order of PID.";
                }
                else if (ascOrDesc == 'd')
                {
                    cout << "Sorting processes by PID in descending order..." << endl;
                    sort(currentProcesses.begin(), currentProcesses.end(), [](const unique_ptr<Process> &a, const unique_ptr<Process> &b)
                         { return a->getPID() > b->getPID(); });
                    sortedBy = "Displayed processes in descending order of PID.";
                }
            }
            else if (sortBy == "ppid")
            {
                if (ascOrDesc == 'a')
                {
                    cout << "Sorting processes by PPID in ascending order..." << endl;
                    sort(currentProcesses.begin(), currentProcesses.end(), [](const unique_ptr<Process> &a, const unique_ptr<Process> &b)
                         { return a->getParentPID() < b->getParentPID(); });
                    sortedBy = "Displayed processes in ascending order of PPID.";
                }
                else if (ascOrDesc == 'd')
                {
                    cout << "Sorting processes by PPID in descending order..." << endl;
                    sort(currentProcesses.begin(), currentProcesses.end(), [](const unique_ptr<Process> &a, const unique_ptr<Process> &b)
                         { return a->getParentPID() > b->getParentPID(); });
                    sortedBy = "Displayed processes in descending order of PPID.";
                }
            }
            else if (sortBy == "name")
            {
                if (ascOrDesc == 'a')
                {
                    cout << "Sorting processes by name in ascending order..." << endl;
                    sort(currentProcesses.begin(), currentProcesses.end(), [](const unique_ptr<Process> &a, const unique_ptr<Process> &b)
                         { return a->getName() < b->getName(); });
                    sortedBy = "Displayed processes in ascending order of name.";
                }
                else if (ascOrDesc == 'd')
                {
                    cout << "Sorting processes by name in descending order..." << endl;
                    sort(currentProcesses.begin(), currentProcesses.end(), [](const unique_ptr<Process> &a, const unique_ptr<Process> &b)
                         { return a->getName() > b->getName(); });
                    sortedBy = "Displayed processes in descending order of name.";
                }
            }
            else if (sortBy == "cpu")
            {
                if (ascOrDesc == 'a')
                {
                    cout << "Sorting processes by CPU usage in ascending order..." << endl;
                    sort(currentProcesses.begin(), currentProcesses.end(), [](const unique_ptr<Process> &a, const unique_ptr<Process> &b)
                         { return a->getCPUUsage() < b->getCPUUsage(); });
                    sortedBy = "Displayed processes in ascending order of CPU usage.";
                }
                else if (ascOrDesc == 'd')
                {
                    cout << "Sorting processes by CPU usage in descending order..." << endl;
                    sort(currentProcesses.begin(), currentProcesses.end(), [](const unique_ptr<Process> &a, const unique_ptr<Process> &b)
                         { return a->getCPUUsage() > b->getCPUUsage(); });
                    sortedBy = "Displayed processes in descending order of CPU usage.";
                }
            }
            else
            {
                cout << "Invalid sort option. Please try again." << endl;
            }
            displayProcesses(currentProcesses);
            cout << sortedBy << endl;
        }
        else if (command == "filter")
        {
            vector<Process *> filteredProcesses;
            string filterBy;
            cout << "Filter by: (memory/priority/name/owner/cpu) " << endl;
            cin >> filterBy;

            if (filterBy == "memory")
            {
                double threshold;
                cout << "Enter memory usage threshold (%) as a decimal (e.g., 0.5 for 0.5%): ";
                cin >> threshold;

                // Create a vector of raw pointers for filtered view
                for (const auto &proc : currentProcesses)
                {
                    if (proc->getMemoryUsage() > threshold)
                    {
                        filteredProcesses.push_back(proc.get());
                    }
                }
            }
            else if (filterBy == "priority")
            {
                int threshold;
                cout << "Enter priority threshold: ";
                cin >> threshold;

                // Create a vector of raw pointers for filtered view
                for (const auto &proc : currentProcesses)
                {
                    if (proc->getPriority() > threshold)
                    {
                        filteredProcesses.push_back(proc.get());
                    }
                }
            }
            else if (filterBy == "name")
            {
                string nameFilter;
                cout << "Enter name filter: ";
                cin >> nameFilter;

                // Create a vector of raw pointers for filtered view
                for (const auto &proc : currentProcesses)
                {
                    if (proc->getName().find(nameFilter) != string::npos)
                    {
                        filteredProcesses.push_back(proc.get());
                    }
                }
            }
            else if (filterBy == "owner")
            {
                string ownerFilter;
                cout << "Enter owner filter: ";
                cin >> ownerFilter;

                // Create a vector of raw pointers for filtered view
                for (const auto &proc : currentProcesses)
                {
                    if (proc->getOwner().find(ownerFilter) != string::npos)
                    {
                        filteredProcesses.push_back(proc.get());
                    }
                }
            }
            else if (filterBy == "cpu")
            {
                double threshold;
                cout << "Enter CPU usage threshold (%) as a decimal (e.g., 0.5 for 0.5%): ";
                cin >> threshold;

                // Create a vector of raw pointers for filtered view
                for (const auto &proc : currentProcesses)
                {
                    if (proc->getCPUUsage() > threshold)
                    {
                        filteredProcesses.push_back(proc.get());
                    }
                }
            }
            else
            {
                cout << "Invalid filter option. Please try again." << endl;
                continue;
            }
            vector<unique_ptr<Process>> tempProcesses;
            for (Process *proc : filteredProcesses)
            {
                tempProcesses.push_back(unique_ptr<Process>(new Process(*proc)));
            }
            displayProcesses(tempProcesses);
            cout << "Filtered processes displayed." << endl;
        }
        else if (command == "terminate")
        {
            int pidToTerminate;
            cout << "Enter PID to terminate: ";
            cin >> pidToTerminate;

            if (kill(pidToTerminate, SIGTERM) == 0) // if the termination was successful
            {
                cout << "Process " << pidToTerminate << " terminated with SIGTERM." << endl;
            }
            else
            {
                perror("SIGTERM failed");
                cout << "Do you want to force kill the process using SIGKILL (kill -9)? (y/n): ";
                char choice;
                cin >> choice;
                if (choice == 'y' || choice == 'Y')
                {
                    if (kill(pidToTerminate, SIGKILL) == 0)
                    {
                        cout << "Process " << pidToTerminate << " forcefully terminated with SIGKILL." << endl;
                    }
                    else
                    {
                        perror("SIGKILL also failed");
                    }
                }
                else
                {
                    cout << "Process was not forcefully terminated." << endl;
                }
            }
        }
        else if (command == "group")
        {
            cout << "Group by (owner/parent): ";
            string groupType;
            cin >> groupType;

            if (groupType == "owner")
            {
                map<string, vector<Process *>> ownerGroups;
                for (const auto &proc : currentProcesses)
                {
                    ownerGroups[proc->getOwner()].push_back(proc.get());
                }

                cout << "Grouped by owner:\n\n";
                for (const auto &[owner, group] : ownerGroups)
                {
                    cout << "[+] " << owner << " (" << group.size() << " processes)" << endl;
                }
                cout << "\nType 'expand owner [name]' to view details.\n";
            }
            else if (groupType == "parent")
            {
                map<int, vector<Process *>> parentMap;
                for (const auto &proc : currentProcesses)
                {
                    parentMap[proc->getParentPID()].push_back(proc.get());
                }

                cout << "Grouped by parent PID:\n\n";
                for (const auto &[ppid, group] : parentMap)
                {
                    cout << "[+] PID " << ppid << " (" << group.size() << " children)" << endl;
                }
                cout << "\nType 'expand pid [pid]' to view children.\n";
            }
            else
            {
                cout << "Invalid group type. Use 'owner' or 'parent'." << endl;
            }
        }
        else if (command.substr(0, 13) == "expand owner ")
        {
            string ownerName = command.substr(13);
            map<string, vector<Process *>> ownerGroups;
            for (const auto &proc : currentProcesses)
            {
                ownerGroups[proc->getOwner()].push_back(proc.get());
            }

            if (ownerGroups.find(ownerName) != ownerGroups.end())
            {
                cout << "\nProcesses owned by: " << ownerName << "\n";
                for (const auto *proc : ownerGroups[ownerName])
                {
                    cout << "  PID " << proc->getPID() << " | Name: " << proc->getName()
                         << " | PPID: " << proc->getParentPID() << endl;
                }
            }
            else
            {
                cout << "Owner group not found." << endl;
            }
        }
        else if (command.substr(0, 11) == "expand pid ")
        {
            int parentPid = stoi(command.substr(11));
            map<int, vector<Process *>> parentMap;
            for (const auto &proc : currentProcesses)
            {
                parentMap[proc->getParentPID()].push_back(proc.get());
            }

            if (parentMap.find(parentPid) != parentMap.end())
            {
                cout << "\nChildren of PID " << parentPid << ":\n";
                for (const auto *proc : parentMap[parentPid])
                {
                    cout << "  PID " << proc->getPID() << " | Name: " << proc->getName()
                         << " | Owner: " << proc->getOwner() << endl;
                }
            }
            else
            {
                cout << "No children found for PID " << parentPid << "." << endl;
            }
        }
        else if (!command.empty())
        {
            cout << "Unknown command: '" << command << "'. Type 'help' for options." << endl;
        }
    }
    cout << "Exiting LPM." << endl;
    return 0;
}