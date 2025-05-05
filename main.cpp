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
using namespace std;

bool running = true;           // flag for controlling auto-refresh
void signalHandler(int signum) // signal handler for Ctrl+C
{
    running = false;
}

void clearScreen() // clear the screen function
{
    cout << "\033[2J\033[1;1H"; // escape sequence to clear screen
}

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
    unsigned long utimePrevious;
    unsigned long stimePrevious;
    unsigned long utimeCurrent;
    unsigned long stimeCurrent;
    double cpuUsage;
    static long clk_tck;
    static int num_cpus;
    static unsigned long prevTotalJiffies;
    static unsigned long prevBusyJiffies;

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
        utimePrevious = utimeCurrent;
        stimePrevious = stimeCurrent;

        // read process info from /proc/[pid]/stat
        string statFilePath = "/proc/" + to_string(pid) + "/stat";
        ifstream statFile(statFilePath);
        if (statFile.is_open())
        {
            string line;
            getline(statFile, line);

            // parse process stat line
            size_t firstParen = line.find('(');
            size_t lastParen = line.rfind(')');

            if (firstParen != string::npos && lastParen != string::npos)
            {
                // Extract name
                name = line.substr(firstParen + 1, lastParen - firstParen - 1);

                // Extract other fields after the closing parenthesis
                string rest = line.substr(lastParen + 2); // +2 to skip ') '
                stringstream ss(rest);
                string token;

                // Read tokens up to utime (14th field after name)
                char state;
                ss >> state; // 3: Process state
                status = state;

                ss >> ppid; // 4: Parent PID

                // Skip to priority (14th field)
                for (int i = 5; i <= 13; i++)
                {
                    if (!(ss >> token))
                    {
                        break;
                    }
                }

                ss >> utimeCurrent; // 14: user time
                ss >> stimeCurrent; // 15: system time
                ss >> token;        // 16: cutime
                ss >> token;        // 17: cstime
                ss >> priority;     // 18: priority
            }
        }
        statFile.close();

        // Read memory info from /proc/[pid]/status
        string statusFilePath = "/proc/" + to_string(pid) + "/status";
        ifstream statusFile(statusFilePath);
        if (statusFile.is_open())
        {
            string line;
            while (getline(statusFile, line))
            {
                if (line.find("Uid:") == 0)
                {
                    // extract the first UID
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
                    double memKB = stod(line.substr(7));
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
    explicit Process(int p) : pid(p)
    {
        fetchProcessDetails();
    }

    int getPID() const { return pid; }
    const string &getName() const { return name; }
    double getMemoryUsage() const { return memoryUsage; }
    const string &getOwner() const { return owner; }
    int getParentPID() const { return ppid; }
    const string &getStatus() const { return status; }
    int getPriority() const { return priority; }
};

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
    vector<unique_ptr<Process>> processesFound; // Vector to store results

    DIR *processesDirectory = opendir("/proc"); // open /proc directory
    if (!processesDirectory)
    { // if it doesn't open report error
        cout << "Error opening /proc directory" << endl;
    }

    struct dirent *entry; // directory entry (temporarily hold a pointer to a directory entry)
    while ((entry = readdir(processesDirectory)) != NULL)
    { // reads the next available entry
        if (entry->d_type == DT_DIR)
        {                                  // check if the entry is a directory
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

void displayProcesses(const vector<unique_ptr<Process>> &processList)
{
    cout << left; // Left-align columns
    cout << setw(8) << "PID" << setw(25) << "Name"
         << setw(12) << "Memory(%)" << setw(10) << "Owner"
         << setw(8) << "PPID" << setw(8) << "Status"
         << setw(10) << "Priority" << endl;
    cout << string(81, '-') << endl; // Adjust width as needed

    // Print each process
    for (const auto &procPtr : processList)
    {
        if (!procPtr)
            continue; // safe check just incase

        cout << setw(8) << procPtr->getPID()
             << setw(25) << procPtr->getName().substr(0, 24)
             << fixed << setprecision(1) << setw(12) << procPtr->getMemoryUsage()
             << setw(10) << procPtr->getOwner().substr(0, 9)
             << setw(8) << procPtr->getParentPID()
             << setw(8) << procPtr->getStatus()
             << setw(10) << procPtr->getPriority()
             << endl;
    }
    cout << string(81, '-') << endl;
    cout << "Total Processes: " << processList.size() << endl
         << endl;
}

int main()
{
    signal(SIGINT, signalHandler); // Register signal handler for Ctrl+C

    cout << "--- Simple Linux Process Lister ---" << endl;

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
    cout << " - 'sort': Sort the process list by memory/priority/pid/ppid/name" << endl;
    cout << " - 'exit': Quit the program" << endl;

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
            cout << "  sort    - Sort the process list by memory/priority/pid/ppid/name.\n";
            cout << "  exit    - Quit the program.\n";
        }
        else if (command == "sort")
        {
            string sortBy;
            char ascOrDesc;
            string sortedBy;
            cout << "Sort by: (memory/priority/pid/ppid/name) " << endl;
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
            else
            {
                cout << "Invalid sort option. Please try again." << endl;
            }
            displayProcesses(currentProcesses);
            cout << sortedBy << endl;
        }
        else if (command == "filter")
        {
            vector<Process*> filteredProcesses;
            string filterBy;
            cout << "Filter by: (memory/priority/name/owner) " << endl;
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
            else
            {
                cout << "Invalid filter option. Please try again." << endl;
                continue;
            }
            vector<unique_ptr<Process>> tempProcesses;
            for (Process* proc : filteredProcesses) {
                tempProcesses.push_back(unique_ptr<Process>(new Process(*proc)));
            }
            displayProcesses(tempProcesses);
            cout << "Filtered processes displayed." << endl;
        }
        else if (!command.empty())
        {
            cout << "Unknown command: '" << command << "'. Type 'help' for options." << endl;
        }
    }
    cout << "Exiting LPM." << endl;
    return 0;
}