
---

# Kosaraju-Sharir-Server 

This repository contains solutions for various operating system exercises, focusing on concepts such as process management, threading, and synchronization.

## Structure

The repository is structured into several folders, each corresponding to a different exercise.

### Folders and Exercises:

1. **Q1**:
   - **Description**: Implemented basic process creation and termination.
   - **Details**: Created a child process and ensured proper termination and resource cleanup.

2. **Q2**:
   - **Description**: Developed inter-process communication (IPC) mechanisms.
   - **Details**: Implemented pipes to allow communication between parent and child processes.

3. **Q3**:
   - **Description**: Explored threading concepts.
   - **Details**: Created multiple threads and demonstrated concurrent execution.

4. **Q4**:
   - **Description**: Implemented synchronization using mutexes.
   - **Details**: Ensured thread-safe access to shared resources.

5. **Q5**:
   - **Description**: Used semaphores for process synchronization.
   - **Details**: Coordinated the execution order of multiple processes.

6. **Q6**:
   - **Description**: Developed a simple scheduler.
   - **Details**: Implemented round-robin scheduling to manage process execution.

7. **Q7**:
   - **Description**: Implemented a basic memory management system.
   - **Details**: Demonstrated allocation and deallocation of memory blocks.

8. **Q8**:
   - **Description**: Explored file system interactions.
   - **Details**: Created, read, and wrote to files using system calls.

9. **Q9**:
   - **Description**: Implemented basic socket programming.
   - **Details**: Created a client-server model using sockets for communication.

10. **Q10**:
    - **Description**: Developed a multi-threaded web server.
    - **Details**: Handled multiple client requests concurrently using threads.

## Installation

1. Clone the repository:
    ```bash
    git clone https://github.com/shifaaKh28/Operating_System-EX3.git
    ```
2. Navigate to the project directory:
    ```bash
    cd Operating_System-EX3
    ```

## Compilation

Each exercise can be compiled using the provided `Makefile`. Run the following command to compile:
```bash
make
```

## Usage

Navigate to the specific exercise folder and run the compiled executable. For example, to run the first exercise:
```bash
cd Q1
./q1_executable
```

## Contributing

Contributions are welcome! Please open an issue or submit a pull request.

---
