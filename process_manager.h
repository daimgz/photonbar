#ifndef PROCESS_MANAGER_H
#define PROCESS_MANAGER_H

#include <string>
#include <csignal>
#include <sys/types.h>

class ProcessManager {
public:
    ProcessManager();
    ~ProcessManager();
    
    bool handleExistingInstances(bool restartMode, bool verbose);
    void setupSignalHandlers();
    void cleanup();
    bool killExistingInstance(bool verbose);
    
private:
    const char* lockFilePath;
    
    const char* getLockFilePath();
    bool isProcessRunning(pid_t pid);
    pid_t readLockFile();
    bool writeLockFile(pid_t pid);
    void removeLockFile();
    
    static void signalHandler(int signal);
    static ProcessManager* gInstance;
};

#endif