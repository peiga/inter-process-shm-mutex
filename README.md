# Process scoped shared memory mutex

This is an example repository to demonstrate how to use inter-process mutexes allocated on a shared memory segment and mapped to each address space, to achieve inter process communication.

The binary generated from this C++ program, operates as follows:
1. Recursively spawns a hierarchy child processes. 
2. Each child process spawns 1 to 3 children.
3. Without any communication, the growth is unbounded. 
4. A shared segment of memory is allocated to count the number of total processes.
5. Additionally, a pthread mutex with the `PTHREAD_PROCESS_SHARED` attribute is also allocated into shared memory.
6. Before spawning new child processes, each process maps the shared segment into it's address space, consults this mutex, and spawns children only if the counter does not exceed a predefined ceiling.

# Running the program
Run `make` to create the binary and `make clean` to clean build artifacts. Alternatively,
use Docker to run the application as follows:
```
$ docker build -t ipcmtx .

$ docker run -it ipcmtx /usr/src/app/binary
```


# Caveats and warnings
1. Careful to tweak the program. The unbounded process growth can literally bring down the system to a halt.
2. Note the system value of maximum memory that can be segmented into shared space. If this value is exceeded, the `shmget` syscall fails with
`No space left on device`.
3. A few versions of Clang/GCC were used. Thorough portable compilation is not guaranteed.

# License 
MIT
