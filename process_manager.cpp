#include "process_manager.h"
#include <unistd.h>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>
#include <dirent.h>
#include <pwd.h>

ProcessManager* ProcessManager::gInstance = nullptr;

ProcessManager::ProcessManager() : lockFilePath(getLockFilePath()) {
  gInstance = this;
}

ProcessManager::~ProcessManager() {
  if (gInstance == this) {
    gInstance = nullptr;
  }
}

const char* ProcessManager::getLockFilePath() {
  static char lockPath[256];
  struct passwd *pw = getpwuid(getuid());
  snprintf(lockPath, sizeof(lockPath), "/tmp/myBar_%s.lock", pw->pw_name);
  return lockPath;
}

bool ProcessManager::isProcessRunning(pid_t pid) {
  return kill(pid, 0) == 0;
}

pid_t ProcessManager::findRunningProcess() {
  pid_t myPid = getpid();
  pid_t ppid = getppid();

  std::string myExe;
  {
    char buf[256];
    ssize_t len = readlink("/proc/self/exe", buf, sizeof(buf) - 1);
    if (len > 0) {
      buf[len] = '\0';
      myExe = buf;
      size_t slash = myExe.rfind('/');
      if (slash != std::string::npos) myExe = myExe.substr(slash + 1);
    }
  }

  DIR* procDir = opendir("/proc");
  if (!procDir) return 0;

  struct dirent* entry;
  while ((entry = readdir(procDir)) != nullptr) {
    std::string name(entry->d_name);
    if (name.find_first_not_of("0123456789") != std::string::npos) continue;

    pid_t pid = std::stoi(name);
    if (pid == myPid || pid == ppid) continue;

    std::string exePath = "/proc/" + name + "/exe";
    char buf[256];
    ssize_t len = readlink(exePath.c_str(), buf, sizeof(buf) - 1);
    if (len <= 0) continue;
    buf[len] = '\0';

    std::string exeName(buf);
    size_t slash = exeName.rfind('/');
    if (slash != std::string::npos) exeName = exeName.substr(slash + 1);

    if (exeName == "photonbar" && isProcessRunning(pid)) {
      closedir(procDir);
      return pid;
    }
  }
  closedir(procDir);
  return 0;
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
    existingPid = findRunningProcess();
    if (existingPid == 0) {
      if (verbose) {
        printf("No se encontró instancia existente de photonbar\n");
      }
      return false;
    }
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
    printf("Recibida señal %d, terminando photonbar...\n", signal);
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
    existingPid = findRunningProcess();
    if (existingPid == 0) {
      if (!writeLockFile(getpid())) {
        fprintf(stderr, "Error: No se pudo crear lock file\n");
        return false;
      }
      return true;
    }
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
      printf("Error: photonbar ya está en ejecución (PID: %d)\n", existingPid);
      printf("Usa --restart para terminar la instancia existente y empezar una nueva\n");
    }
    return false;
  }
}

void ProcessManager::cleanup() {
  removeLockFile();
}