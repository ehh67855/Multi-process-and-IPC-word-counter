# Multi process and IPC word counter
This C program performs a word count on a specified file by dividing the work among multiple child processes. It's designed to demonstrate inter-process communication (IPC), process management, file operations, and basic fault tolerance in a parallel computing context within C. The program measures the performance of the operation in terms of execution time and provides detailed output including the total number of lines, words, and characters found in the file. large.txt is used to test the scalability of the program. 

## Program structure:
The program divides the input file into segments based on the number of child proccesses needed. It then establishes a pipe for IPC with each child process.
A fork command is used to create the child proccesses, and each proccess reads its assigned portion. Then the main proccess reads from the pipes nd aggregates the total word count from all children proccesses. The parent proccess monitors the exit status of each child process using waitpid().
If a child proccess crashes, then a new one is created and the same procedue from before is used to recalculate the wordcount from that portion of the input file.
Then the final results and details on timing are outputted.

## IPC:
The program makes use of pipes to help the parent and child processes communicate with each other. For every child process, a different pipe is made. Child processes can communicate back to the parent process with their word count results via pipes. In order to aggregate the results the parent proccess reads from the read end of the pipe
- **Pipe Creation:** Prior to the fork operation, a pipe is created for every child process. This guarantees that the pipe is accessible to both the parent and the child processes.
- **Data Transmission:** Child processes write their word count results to the write end and close the read end of their corresponding pipes. The write end is closed to indicate that the data transmission has ended after writing the results.
- **Result Aggregation:** The parent process reads the word count results from the read end and closes the write end of each pipe. It aggregates the results as it reads them to determine the overall word count.

## Crash Handling:
- **Structure:** The waitpid() system call is used by the parent process to keep track of each child process's exit status.
The parent can ascertain whether a child process ended normally or as a result of a crash by looking at the exit status.
The parent process takes the subsequent actions to recover from the failure after detecting a crash:
In order to free up resources, the parent shuts off the two ends of the pipe connected to the crashed child process.
- **Task Reassignment:** The input file segment assigned to the crashed process is calculated by the parent.
After that, it makes a new child process, gives it the same segment, and configures an IPC pipe.
The newly created child process counts the words as it reads the segment it is allocated from the file.
To make sure that no portion of the file is left unprocessed, the results are routed via the new pipe back to the parent process.
The final word count is guaranteed to be accurate even in the event of child process failures thanks to this crash-handling mechanism.
The program keeps its integrity and produces accurate results by automatically retrying the task assigned to crashed processes.