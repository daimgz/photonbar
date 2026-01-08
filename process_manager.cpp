#include "process_manager.h"
#include <unistd.h>
#include <pwd.h>
#include <fstream>
#include <cstdio>
#include <cstdlib>

ProcessManager* ProcessManager::gInstance = nullptr;

ProcessManager::ProcessManager() : lockFilePath(nullptr) {
    gInstance = this;
}

ProcessManager::~ProcessManager() {
    cleanup();
}

const char* ProcessManager::getLockFilePath() {
    static char lockPath[512];
    struct passwd *pw = getpwuid(getuid());
    snprintf(lockPath, sizeof(lockPath), "/tmp/myBar_%s.lock", pw->pw_name);
    return lockPath;
}

bool ProcessManager::isProcessRunning(pid_t pid) {
    return kill(pid, 0) == 0;
}

pid_t ProcessManager::readLockFile() {
    std::ifstream lock_file(lockFilePath);
    if (!lock_file.is_open()) {
        return 0;
    }

    pid_t pid;
    lock_file >> pid;
    lock_file.close();

    return pid;
}

bool ProcessManager::writeLockFile(pid_t pid) {
    std::ofstream lock_file(lockFilePath);
    if (!lock_file.is_open()) {
        return false;
    }

    lock_file << pid;
    lock_file.close();
    return true;
}

void ProcessManager::removeLockFile() {
    unlink(lockFilePath);
}

bool ProcessManager::killExistingInstance(bool verbose) {
    pid_t existingPid = readLockFile();

    if (existingPid == 0) {
        if (verbose) {
            printf("No se encontró instancia existente de myBar\n");
        }
        return false;
    }

if (!isProcessRunning(existingPid)) {
            if (verbose) {
                printf("Se encontró lock file stale, eliminando...\n");
            }
            removeLockFile();
            return false;
        }

        if (verbose) {
            printf("Terminando instancia existente (PID: %d)...\n", existingPid);
        }

        kill(existingPid, SIGTERM);
    sleep(2);

if (isProcessRunning(existingPid)) {
            if (verbose) {
                printf("La instancia no terminó, forzando cierre...\n");
            }
            kill(existingPid, SIGKILL);
        sleep(1);
    }

    removeLockFile();

    if (verbose) {
        printf("Instancia existente terminada.\n");
    }

    return true;
}

void ProcessManager::signalHandler(int signal) {
    if (gInstance) {
        printf("Recibida señal %d, terminando myBar...\n", signal);
        gInstance->cleanup();
        exit(0);
    }
}

void ProcessManager::setupSignalHandlers() {
    signal(SIGTERM, signalHandler);
    signal(SIGINT, signalHandler);
    signal(SIGHUP, SIG_IGN);
}

bool ProcessManager::handleExistingInstances(bool restartMode, bool verbose) {
    lockFilePath = getLockFilePath();

    pid_t existingPid = readLockFile();

    if (existingPid == 0) {
        if (!writeLockFile(getpid())) {
            fprintf(stderr, "Error: No se pudo crear lock file\n");
            return false;
        }
        return true;
    }

if (!isProcessRunning(existingPid)) {
            if (verbose) {
                printf("Lock file stale detectado (PID %d no existe), eliminando...\n", existingPid);
        }
        removeLockFile();

        if (!writeLockFile(getpid())) {
            fprintf(stderr, "Error: No se pudo crear lock file\n");
            return false;
        }
        return true;
    }

    if (restartMode) {
        if (verbose) {
            printf("Modo restart: Terminando instancia existente...\n");
        }
        killExistingInstance(verbose);

        sleep(1);
        if (!writeLockFile(getpid())) {
            fprintf(stderr, "Error: No se pudo crear lock file después de restart\n");
            return false;
        }
        return true;
    } else {
        if (verbose) {
            printf("Error: myBar ya está en ejecución (PID: %d)\n", existingPid);
            printf("Usa --restart para terminar la instancia existente y empezar una nueva\n");
        }
        return false;
    }
}

void ProcessManager::cleanup() {
    removeLockFile();
}