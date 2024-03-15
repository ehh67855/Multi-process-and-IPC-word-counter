#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <time.h>
#include "wc.h" 

int main(int argc, char **argv) {
    long fsize;
    FILE *fp;
    struct timespec begin, end;
    int nChildProc = 0;

    /* 1st arg: filename */
    if(argc <= 2) {
            printf("usage: wc <filname> [# processes] [crash rate]\n");
            return 0;
    }

    /* 2nd (optional) arg: number of child processes */
    if (argc > 2) {
            nChildProc = atoi(argv[2]);
            if(nChildProc < 1) nChildProc = 1;
            if(nChildProc > 10) nChildProc = 10;
    }

	/* 3rd (optional) arg: crash rate between 0% and 100%. Each child process has that much chance to crash*/
    if(argc > 3) {
            crashRate = atoi(argv[3]);
            if(crashRate < 0) crashRate = 0;
            if(crashRate > 50) crashRate = 50;
            printf("crashRate RATE: %d\n", crashRate);
    }

    printf("# of Child Processes: %d\n", nChildProc);
    printf("crashRate RATE: %d\n", crashRate);

    // start to measure time
    clock_gettime(CLOCK_REALTIME, &begin);

    // Open file in read-only mode
    fp = fopen(argv[1], "r");

    if(fp == NULL) {
            printf("File open error: %s\n", argv[1]);
            printf("usage: wc <filname>\n");
            return 0;
    }

    // get a file size
    fseek(fp, 0, SEEK_END);
    fsize = ftell(fp);

    // Divide the file into segments for each child process.
    long segmentSize = fsize / nChildProc;
    long offset = 0;

    // Pipes for IPC
    int pipes[nChildProc][2];
    //pids and offsets stored for later use
    pid_t pids[nChildProc];
    long offsets[nChildProc];


    for (int i = 0; i < nChildProc; ++i) {

        //creatin n pipes
        if (pipe(pipes[i]) == -1) {
            perror("pipe");
            exit(1);
        }

        pid_t pid = fork();
        pids[i] = pid;
        if (pid == 0) { // Child process
            fclose(fp); 
            FILE *fp_child = fopen(argv[1], "r");
            if (!fp_child) {
                perror("File opening failed in child");
                exit(1);
            }

            // Adjust the offset for the last child process to read till the end of the file.
            long size;
            if (i == nChildProc - 1) {
                size = fsize - offset;
            } else {
                size = segmentSize;
            }

            count_t localCount = word_count(fp_child, offset, size);

            // Write the count to the pipe and exit.
            close(pipes[i][0]); 
            write(pipes[i][1], &localCount, sizeof(count_t));
            close(pipes[i][1]); 
            fclose(fp_child);
            exit(0);
        } else if (pid < 0) {
            perror("fork");
            exit(1);
        }
        //Store offset and increment it for next child
        offsets[i] = offset;
        offset += segmentSize;
    }

    count_t total = {0, 0, 0};
    for (int i = 0; i < nChildProc; ++i) {
        int status;
        pid_t childPid;

        while ((childPid = waitpid(pids[i], &status, 0)) > 0) {
            if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
                count_t childCount;
                close(pipes[i][1]);
                read(pipes[i][0], &childCount, sizeof(count_t));
                total.linecount += childCount.linecount;
                total.wordcount += childCount.wordcount;
                total.charcount += childCount.charcount;
                close(pipes[i][0]); 
                break; // Exit the loop if the child completed successfully
            } else if (WIFSIGNALED(status)) { // Child crashed
                // Close the old pipe
                close(pipes[i][0]);
                close(pipes[i][1]);

                if (pipe(pipes[i]) == -1) {
                    perror("pipe");
                    exit(1);
                }
                pids[i] = fork();
                if (pids[i] == 0) {
                    FILE *fp_child = fopen(argv[1], "r");
                    if (!fp_child) {
                        perror("File opening failed in child");
                        exit(1);
                    }

                    long size;
                    if (i == nChildProc -1) {
                        size = fsize - offsets[i];
                    } else {
                        size = segmentSize;
                    }
                    fseek(fp_child, offsets[i], SEEK_SET);
                    count_t retryCount = word_count(fp_child, offsets[i], size);

                    close(pipes[i][0]);
                    write(pipes[i][1], &retryCount, sizeof(count_t));
                    close(pipes[i][1]);
                    fclose(fp_child);
                    exit(0);
                } else if (pids[i] < 0) {
                    perror("fork");
                    exit(1);
                }
            }
        }
    }


    clock_gettime(CLOCK_REALTIME, &end);
    long seconds = end.tv_sec - begin.tv_sec;
    long nanoseconds = end.tv_nsec - begin.tv_nsec;
    double elapsed = seconds + nanoseconds * 1e-9;

    printf("\n========= %s =========\n", argv[1]);
    printf("Total Lines: %d\n", total.linecount);
    printf("Total Words: %d\n", total.wordcount);
    printf("Total Characters: %d\n", total.charcount);
    printf("Time taken: %.3f seconds\n", elapsed);

    fclose(fp);
    return 0;
}