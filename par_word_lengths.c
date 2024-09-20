#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#define MAX_WORD_LEN 25

/*
 * Counts the number of occurrences of words of different lengths in a text
 * file and stores the results in an array.
 * file_name: The name of the text file from which to read words
 * counts: An array of integers storing the number of words of each possible
 *     length.  counts[0] is the number of 1-character words, counts [1] is the
 *     number of 2-character words, and so on.
 * Returns 0 on success or -1 on error.
 */
int count_word_lengths(const char *file_name, int *counts) {
    FILE* fp = fopen(file_name, "r");
    if (fp == NULL) {
        perror("fopen");
        return -1;
    }

    char word[MAX_WORD_LEN + 1];
    int len = 0;
    while (fscanf(fp, "%s", word) == 1) {
        len = strlen(word);
        if (len <= MAX_WORD_LEN) {
            counts[len - 1]++;
        }
    }

    if (fclose(fp) == -1) {
        perror("fclose");
        return -1;
    }

    return 0;
}

/*
 * Processes a particular file (counting the number of words of each length)
 * and writes the results to a file descriptor.
 * This function should be called in child processes.
 * file_name: The name of the file to analyze.
 * out_fd: The file descriptor to which results are written
 * Returns 0 on success or -1 on error
 */
int process_file(const char *file_name, int out_fd) {
    int counts[MAX_WORD_LEN] = {0};

    count_word_lengths(file_name, counts);

    if (write(out_fd, counts, sizeof(counts)) == -1) {
        perror("write");
        return -1;
    }
    return 0;
}

int main(int argc, char **argv) {
    if (argc == 1) {
        // No files to consume, return immediately
        return 0;
    }

    // TODO Create a pipe for child processes to write their results
    int counts[MAX_WORD_LEN] = {0}; // initialize counts to 0
    int pipefd[2];
    int status;

    if (pipe(pipefd) == -1) {
        perror("pipe");
        return -1;
    }   

    // TODO Fork a child to analyze each specified file (names are argv[1], argv[2], ...)
    for (int i = 1; i < argc; i++) {
        pid_t pid = fork();
        if (pid == -1) {
            perror("fork");
            return -1;
        } else if (pid == 0) {
            // Child process
            if (close(pipefd[0]) == -1) {
                perror("close read end of pipe");
                return -1;
            }

            if (process_file(argv[i], pipefd[1]) == -1) {
                perror("process file");
                exit(EXIT_FAILURE);
            }

            if (close(pipefd[1]) == -1) {
                perror("close write end of pipe");
                return -1;
            }
            exit(EXIT_SUCCESS);
        }
    }

    // TODO Aggregate all the results together by reading from the pipe in the parent
    // Parent process
    
    if (close(pipefd[1]) == -1) {
        perror("close write end of pipe");
        return -1;
    }

    for (int i = 1; i < argc; i++) {
        int child[MAX_WORD_LEN] = {0};

        if (wait(&status) == -1) {
            perror("wait");
            return -1;
        }
        if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
            perror("child process");
            return -1;
        }
        
        if (read(pipefd[0], child, MAX_WORD_LEN * sizeof(int)) == -1) {
            perror("read");
            return -1;
        }

        // into a single array of counts
        for (int j = 0; j < MAX_WORD_LEN; j++) {
            counts[j] += child[j];
        }
    }


    // TODO Change this code to print out the total count of words of each length

    for (int i = 1; i <= MAX_WORD_LEN; i++) {
        printf("%d-Character Words: %d\n", i, counts[i-1]);
    }

    if (close(pipefd[0]) == -1) {
        perror("Parent failed to close read end of pipe");
        return -1;
    }
    
    return 0;
}
