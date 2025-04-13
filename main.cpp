#include <iostream>
#include <dirent.h>  // directory stuff
#include <memory>    // for unique_ptr
#include <vector>    // for vector
#include <string>    // for string
#include <iomanip>   // for std::setw
#include <fstream>   // for file operations
#include <algorithm> // for all_of
#include <sstream>  // for stringstream
using namespace std;

class Process
{
private:
    int pid;
    string name;
    int priority;
    double cpuUsage;
    double memoryUsage;
    string status;
    string owner;
    int ppid;
    double uTimeTicks;
    double sTimeTicks;

    void fetchProcessDetails()
    {
        name = "N/A";
        priority = 0;
        uTimeTicks = 0;
        sTimeTicks = 0;
        memoryUsage = 0;
        status = '?';
        owner = "N/A";
        ppid = 0;
        uid_t uid_num = -1;
        string statusFilePath = "/proc/" + to_string(pid) + "/status"; // Fetch process details from /proc/[pid]/stat
        ifstream statusFile(statusFilePath);
        if (statusFile.is_open())
        {
            string line;
            while (getline(statusFile, line))
            {
                if (line.find("Name:") == 0)
                {
                    name = line.substr(6); // Extract process name
                }
                else if (line.find("State:") == 0)
                {
                    status = line.substr(7); // Extract process status
                }
                else if (line.find("PPid:") == 0)
                {
                    ppid = stoi(line.substr(6)); // Extract parent process ID
                }
                else if (line.find("Uid:") == 0)
                {
                    owner = line.substr(6); // Extract owner UID
                }
                else if (line.find("VmRSS:") == 0)
                {
                    memoryUsage = stod(line.substr(7)); // Extract memory usage
                }
            }
        }
        statusFile.close();

        string statFilePath = "/proc/" + to_string(pid) + "/stat";
        ifstream statFile(statFilePath);
        if (statFile.is_open())
        {
            string line;
            getline(statFile, line);
            stringstream ss(line);
            string token;
            int fieldIndex = 0;
            while (ss >> token)
            {
                if (fieldIndex == 18)
                { // priority
                    priority = stoi(token);
                }
                else if (fieldIndex == 14)
                { // uTime
                    uTimeTicks = stod(token);
                }
                else if (fieldIndex == 15)
                { // sTime
                    sTimeTicks = stod(token);
                }
                fieldIndex++;
            }
        }
        statFile.close();
    }

public:
    explicit Process(int p) : pid(p)
    {
        fetchProcessDetails(); // Populate data immediately
    }

    int getPID() const { return pid; }
    const string &getName() const { return name; }
    // CPU % getter needs calculation elsewhere based on ticks
    size_t getMemoryUsage() const { return memoryUsage; } // In KB
    const string &getOwner() const { return owner; }
    int getParentPID() const { return ppid; }
    const string &getStatus() const { return status; }
    int getPriority() const { return priority; }
    unsigned long getUserTimeTicks() const { return uTimeTicks; }
    unsigned long getSystemTimeTicks() const { return sTimeTicks; }
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
                    processesFound.push_back(unique_ptr<Process>(new Process(pid))); // C++11 way // create a new Process object and add it to the vector
                }
            }
        }
    }
    closedir(processesDirectory); // close the directory
    return processesFound;        // return the vector of processes
}

void displayProcesses(const vector<unique_ptr<Process>> &processList)
{
    // Print header
    cout << left; // Left-align columns
    cout << setw(8) << "PID" << setw(25) << "Name"
         << setw(12) << "Memory(KB)" << setw(10) << "Owner"
         << setw(8) << "PPID" << setw(8) << "Status"
         << setw(10) << "Priority" << endl;
    cout << string(81, '-') << endl; // Adjust width as needed

    // Print each process
    for (const auto &procPtr : processList)
    {
        if (!procPtr)
            continue; // Should not happen with make_unique, but safe check

        cout << setw(8) << procPtr->getPID()
             << setw(25) << procPtr->getName().substr(0, 24) // Truncate name if needed
             << setw(12) << procPtr->getMemoryUsage()
             << setw(10) << procPtr->getOwner().substr(0, 9)
             << setw(8) << procPtr->getParentPID()
             << setw(8) << procPtr->getStatus()
             << setw(10) << procPtr->getPriority()
             // Add CPU% later once calculated
             << endl;
    }
    cout << string(81, '-') << endl;
    cout << "Total Processes: " << processList.size() << endl
         << endl;
}

int main()
{
    cout << "--- Simple Linux Process Lister ---" << endl;

    // 1. Get the initial list of processes
    cout << "Fetching process list..." << endl;
    vector<unique_ptr<Process>> currentProcesses = findProcesses();

    if (currentProcesses.empty())
    {
        cerr << "No processes found or error reading /proc." << endl;
        return 1; // Indicate error
    }

    // 2. Display the list
    cout << "Displaying processes..." << endl;
    displayProcesses(currentProcesses);

    // 3. Basic command loop placeholder (as required by project spec)
    cout << "Enter command (e.g., 'refresh', 'exit'):" << endl;
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
        else if (command == "help")
        {
            cout << "Available commands:\n";
            cout << "  refresh - Reload and display the process list.\n";
            cout << "  exit    - Quit the program.\n";
            // Add help for sort, filter, kill later
        }
        // --- Add handlers for sort, filter, group, kill commands here ---
        // else if (command.substr(0, 8) == "sort cpu") { ... }
        // else if (command.substr(0, 8) == "sort mem") { ... }
        // else if (command.substr(0, 6) == "filter") { ... }
        // else if (command.substr(0, 4) == "kill") { ... }
        // else if (command == "group parent") { ... }
        // else if (command == "group owner") { ... }
        else if (!command.empty())
        {
            cout << "Unknown command: '" << command << "'. Type 'help' for options." << endl;
        }
    }

    cout << "Exiting LPM." << endl;
    return 0; // Indicate successful execution
}