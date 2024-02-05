#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <time.h>
#include "wc.h" 

int main(int argc, char **argv) {
    long fsize;
    FILE *fp;

    /* 1st arg: filename */
    if(argc < 2) {
            printf("usage: wc <filname> [# processes] [crash rate]\n");
            return 0;
    }

    // File handling
    fp = fopen(argv[1], "r");
    if(fp == NULL) {
            printf("File open error: %s\n", argv[1]);
            printf("usage: wc <filname>\n");
            return 0;
    }

    // Determine the size of the file.
    fseek(fp, 0, SEEK_END);
    fsize = ftell(fp);
    rewind(fp);

    // Determine the number of child processes to create.
    int nChildProc = argc > 2 ? atoi(argv[2]) : 1;
    if (nChildProc < 1) nChildProc = 1;

    // Crash rate handling (not shown here for brevity).
    if(argc > 3) {
            crashRate = atoi(argv[3]);
            if(crashRate < 0) crashRate = 0;
            if(crashRate > 50) crashRate = 50;
            printf("crashRate RATE: %d\n", crashRate);
    }
    // Divide the file into segments for each child process.
    long segmentSize = fsize / nChildProc;
    long offset = 0;

    // Pipes for parent-child communication.
    int pipes[nChildProc][2];
    pid_t pids[nChildProc];
    long offsets[nChildProc];

    // Time measurement variables.
    struct timespec begin, end;
    clock_gettime(CLOCK_REALTIME, &begin);

    for (int i = 0; i < nChildProc; ++i) {
        if (pipe(pipes[i]) == -1) {
            perror("pipe");
            exit(1);
        }

        pid_t pid = fork();
        pids[i] = pid;
        if (pid == 0) { // Child process
            fclose(fp); // Close the file descriptor inherited from the parent.

            // Open the file independently in each child to avoid shared file pointer issues.
            FILE *fp_child = fopen(argv[1], "r");
            if (!fp_child) {
                perror("File opening failed in child");
                exit(1);
            }

            // Adjust the offset for the last child process to read till the end of the file.
            long size = (i == nChildProc - 1) ? (fsize - offset) : segmentSize;

            count_t localCount = word_count(fp_child, offset, size);

            // Write the count to the pipe and exit.
            close(pipes[i][0]); // Close unused read end in child.
            write(pipes[i][1], &localCount, sizeof(count_t));
            close(pipes[i][1]); // Close write end after writing.

            fclose(fp_child);
            exit(0); // Child process exits after its work is done.
        } else if (pid < 0) {
            perror("fork");
            exit(1);
        }

        // Parent process continues to the next child.
        offsets[i] = offset;
        offset += segmentSize; // Move the offset for the next child.
    }

    count_t total = {0, 0, 0};
    for (int i = 0; i < nChildProc; ++i) {
        int status;
        pid_t childPid;

        while ((childPid = waitpid(pids[i], &status, 0)) > 0) {
            if (WIFEXITED(status) && WEXITSTATUS(status) == 0) { // Child exited normally.
                count_t childCount;
                close(pipes[i][1]); // Close unused write end in parent.
                read(pipes[i][0], &childCount, sizeof(count_t)); // Read child's count.
                total.linecount += childCount.linecount;
                total.wordcount += childCount.wordcount;
                total.charcount += childCount.charcount;
                close(pipes[i][0]); // Close read end after reading.
                break; // Exit the loop if the child completed successfully
            } else if (WIFSIGNALED(status)) { // Child crashed
                printf("Child with PID %ld crashed. Restarting...\n", (long)childPid);

                // Close the old pipe
                close(pipes[i][0]);
                close(pipes[i][1]);

                // Create a new pipe for the new child
                if (pipe(pipes[i]) == -1) {
                    perror("pipe");
                    exit(1);
                }

                // Fork a new child process to replace the crashed one
                pids[i] = fork();
                if (pids[i] == 0) { // New child process
                    // Child process setup (similar to initial child setup)
                    FILE *fp_child = fopen(argv[1], "r");
                    if (!fp_child) {
                        perror("File opening failed in child");
                        exit(1);
                    }

                    long size = (i == nChildProc - 1) ? (fsize - offsets[i]) : segmentSize;
                    fseek(fp_child, offsets[i], SEEK_SET);

                    count_t retryCount = word_count(fp_child, offsets[i], size);

                    // Write the count to the pipe and exit
                    close(pipes[i][0]); // Close unused read end in child.
                    write(pipes[i][1], &retryCount, sizeof(count_t));
                    close(pipes[i][1]); // Close write end after writing.

                    fclose(fp_child);
                    exit(0); // Child process exits after its work is done.
                } else if (pids[i] < 0) {
                    perror("fork");
                    exit(1);
                }
            }
        }
    }
    // Time measurement and result printing.
    clock_gettime(CLOCK_REALTIME, &end);
    long seconds = end.tv_sec - begin.tv_sec;
    long nanoseconds = end.tv_nsec - begin.tv_nsec;
    double elapsed = seconds + nanoseconds * 1e-9;

    printf("\n========= %s =========\n", argv[1]);
    printf("Total Lines: %d\n", total.linecount);
    printf("Total Words: %d\n", total.wordcount);
    printf("Total Characters: %d\n", total.charcount);
    printf("Time taken: %.3f seconds\n", elapsed);

    fclose(fp); // Close the file in the parent.
    return 0;
}