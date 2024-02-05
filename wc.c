#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <time.h>
#include "wc.h" // Assuming this header defines count_t and the word_count function.

int main(int argc, char **argv) {
    if (argc < 2) {
        printf("Usage: %s <filename> [# processes] [crash rate]\n", argv[0]);
        return 1;
    }

    // File handling
    FILE *fp = fopen(argv[1], "r");
    if (!fp) {
        perror("File opening failed");
        return 1;
    }

    // Determine the size of the file.
    fseek(fp, 0, SEEK_END);
    long fsize = ftell(fp);
    rewind(fp);

    // Determine the number of child processes to create.
    int nChildProc = argc > 2 ? atoi(argv[2]) : 1;
    if (nChildProc < 1) nChildProc = 1;

    // Crash rate handling (not shown here for brevity).

    // Divide the file into segments for each child process.
    long segmentSize = fsize / nChildProc;
    long offset = 0;

    // Pipes for parent-child communication.
    int pipes[nChildProc][2];

    // Time measurement variables.
    struct timespec begin, end;
    clock_gettime(CLOCK_REALTIME, &begin);

    for (int i = 0; i < nChildProc; ++i) {
        if (pipe(pipes[i]) == -1) {
            perror("pipe");
            exit(1);
        }

        pid_t pid = fork();
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
        offset += segmentSize; // Move the offset for the next child.
    }

    // Parent process: wait for children and aggregate results.
    count_t total = {0, 0, 0};
    for (int i = 0; i < nChildProc; ++i) {
        close(pipes[i][1]); // Close unused write end in parent.

        count_t childCount;
        read(pipes[i][0], &childCount, sizeof(count_t)); // Read child's count.
        total.linecount += childCount.linecount;
        total.wordcount += childCount.wordcount;
        total.charcount += childCount.charcount;

        close(pipes[i][0]); // Close read end after reading.
    }

    // Ensure all child processes have finished.
    while (wait(NULL) > 0);

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