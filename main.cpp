#include <unistd.h>
#include <iostream>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/fcntl.h>
#include <pthread.h>
#include <fstream>
#include <sstream>
#include <unordered_map>
#include <vector>
#include <string>
#include <cstring>
#include <assert.h>
#include <sys/shm.h>
#include <sys/wait.h>

#define MAX_PROCESSES 20 // Maximum number of total processes
#define MIN_SPAWN 1 // Min. number that can be spawned by one process
#define MAX_SPAWN 3 // Max. number that can be spawned by one process

const char* OUT_FILE = "out.txt"; // Output file.

// Common file stream that all processes write to.
std::ofstream f;

// Kill the current process with an error message.
void die(int errcode) {
  perror(strerror(errcode));
  exit(1);
}

// Initialize a process-shared mutex.
void initMutex(pthread_mutex_t* m) {
  int err;
  pthread_mutexattr_t mutex_shared_attr;
  if ((err = pthread_mutexattr_init(&mutex_shared_attr))) {
    die(err);
  }

  if ((err = pthread_mutexattr_setpshared(&mutex_shared_attr, PTHREAD_PROCESS_SHARED)) != 0) {
    die(err);
  }

  if ((err = pthread_mutex_init(m, &mutex_shared_attr)) != 0) {
    die(err);
  }
}

// Helper to lock a mutex with error handling.
void lockMtx(pthread_mutex_t* m) {
  int err;
  if ((err = pthread_mutex_lock(m) != 0)) {
    die(err);
  }
}

// Helper to unlock a mutex with error handling.
void unlockMtx(pthread_mutex_t* m) {
  int err;
  if ((err = pthread_mutex_unlock(m) != 0)) {
    die(err);
  }
}

// Initialize output file. Creates it if it does not exist and adds an entry for current process.
void initOutFile() {
  f.open(OUT_FILE);
  f << getppid() << "," << getpid() << std::endl;
}

// Close output stream to the file and remove it.
void cleanOutFile() {
  f.close();
}

// Initializes shared memory to store an integer counter and a mutex. The mutex operates in 
// shared mode.
char* initSharedMem() {
  const int shmsz = sizeof(int) + sizeof(pthread_mutex_t);
  const int shmid = shmget(IPC_PRIVATE, shmsz, 0666);

  // Error case: Segment was not created.
  if (shmid < 0) {
    perror("shmget failed.");
    exit(1);
  }

  char* shmemPtr = (char*) shmat(shmid, 0, 0);
  if (shmemPtr == (char*) -1) {
    perror("shmat failed.");
    exit(1);
  }

  return shmemPtr;
}

// Return shared memory to the system.
void clearSharedMemory(void* shmemPtr) {
  if (shmdt(shmemPtr) == -1) {
    perror("shmdt failed.");
    exit(1);
  }
}

// Generate a random number between min and max.
int rnd(const int min, const int max) {
  return min + (rand() % static_cast<int>(max - min + 1));
}

// Recursively fork until the counter indictes that we have reached or exceeded MAX_PROCESSES.
// All operations are done while acquiring the mutex lock.
void rfork(int* counter, pthread_mutex_t* m) {
  lockMtx(m);

  *counter += 1;

  if (*counter >= MAX_PROCESSES) {
    unlockMtx(m);
    return;
  }

  if (fork() == 0) {
    // Redirect child stdout to dev null. This prevents multiple printing of the process tree.
    int fd = open("/dev/null", O_WRONLY | O_CREAT, 0666);
    dup2(fd, 1);

    // If the number processes we are trying to spawn will make it such that the total
    // processes exceeds MAX_PROCESSES, then reduce the number until we stay under that limit.
    int processesToSpawn = rnd(MIN_SPAWN, MAX_SPAWN + 1);
    while ((processesToSpawn + *counter) > MAX_PROCESSES) {
      processesToSpawn--;
    }

    // Write out entry for this process to file.
    f << getppid() << "," << getpid() << std::endl;

    for (int i = 0; i < 2; i++) {
      unlockMtx(m);
      rfork(counter, m);
    }
  } else {
    unlockMtx(m);
  }
}

// Reads the output file, and parses its output into a simple process tree.
// The file is in the following CSV form
//
//    1234,5678
//    1234,1238
//    1238,9999
//
// where the first element is the PPID, and the second is the child ID.
// We parse it into the following unordered_map
//
//  1234 => [5678, 1238]
//  1238 => [9999]
//
// This is also the form in which we display the result.
void printProcessTree(pthread_mutex_t* m, int* counter) {
  lockMtx(m);

  // A data structure to hold the information from the output file.
  // The key is the PPID, the values are the PIDs of children.
  std::unordered_map<std::string, std::vector<std::string>> processes;

  std::string line;
  std::ifstream f(OUT_FILE);
  if (f.is_open()) {
    // Get each  line.
    while (getline(f, line)) {
      std::stringstream ss(line); 
      std::string token;
      std::vector<std::string> v;

      // Split each line by ,
      while (getline(ss, token, ',')) {
        v.push_back(token); 
      }

      assert(v.size() >= 2);
      processes[v[0]].push_back(v[1]);
    }
  }

  for (auto pair: processes) {
    std::cout << "\t" << pair.first << std::endl;

    for (auto child: pair.second) {
      std::cout << "\t  |-> " << child << std::endl;
    }
  }

  unlockMtx(m);
}

int main() {
  initOutFile();

  // Setup shared memory.
  char* shmem = initSharedMem();

  // Setup counter. Counter starts from position '0' to 'sizeof(int) - 1'
  int* counter = (int*) shmem;
  *counter = 1;

  // Setup mutex. Mutex starts from position 'sizeof(int)' to 'sizeof(pthread_mutex_t) - sizeof(int)'
  pthread_mutex_t* m = (pthread_mutex_t*) &shmem[sizeof(int)];
  initMutex(m);

  // Recursively fork.
  rfork(counter, m);

  // Wait
  int s; 
  wait(&s);

  // Write the process tree to stdout. Note that only the root process does this
  // since the children have their stdout redirected to /dev/null
  printProcessTree(m, counter);

  // Free memory.
  clearSharedMemory(shmem);
  
  // Close and remove output file.
  cleanOutFile();
}
