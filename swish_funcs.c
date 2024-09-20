#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <string.h>

#include "string_vector.h"
#include "swish_funcs.h"

#define MAX_ARGS 10

/*
 * Helper function to run a single command within a pipeline. You should make
 * make use of the provided 'run_command' function here.
 * tokens: String vector containing the tokens representing the command to be
 * executed, possible redirection, and the command's arguments.
 * pipes: An array of pipe file descriptors.
 * n_pipes: Length of the 'pipes' array
 * in_idx: i of the file descriptor in the array from which the program
 *         should read its input, or -1 if input should not be read from a pipe.
 * out_idx: i of the file descriptor in the array to which the program
 *          should write its output, or -1 if output should not be written to
 *          a pipe.
 * Returns 0 on success or -1 on error.
 */
int run_piped_command(strvec_t *tokens, int *pipes, int n_pipes, int in_idx, int out_idx) {
    int stdin_copy = dup(STDIN_FILENO);
    int stdout_copy = dup(STDOUT_FILENO);

    if (in_idx != STDIN_FILENO) {
        dup2(in_idx, STDIN_FILENO);
        close(in_idx);
    }
    if (out_idx != STDOUT_FILENO) {
        dup2(out_idx, STDOUT_FILENO);
        close(out_idx);
    }

    if (run_command(tokens) == -1) {
        perror("run command");
        return -1;
    }

    dup2(stdin_copy, STDIN_FILENO);
    dup2(stdout_copy, STDOUT_FILENO);
    close(stdin_copy);
    close(stdout_copy);

    return 0;
}


int run_pipelined_commands(strvec_t *tokens) {
    int num_pipes = 0;
    int i = 0;
    int cmd_start = 0;
    int cmd_end = 0;
    
    for (int i = 0; i < tokens->length; i++) {
        if (strcmp(tokens->data[i], "|") == 0) {
            num_pipes++;
        }
    }
    
    // array of pipes
    int pipes[2* num_pipes];
    for (int i = 0; i < num_pipes; i++) {
        if (pipe((2*i)+pipes) == -1) {
            perror("pipe");
            return -1;
        }
    }
    
    int in_idx = STDIN_FILENO;
    int out_idx = STDOUT_FILENO;
    while (cmd_end < tokens->length) {
        // Find end of current command
        cmd_end = cmd_start;
        while (cmd_end < tokens->length && strcmp(tokens->data[cmd_end], "|") != 0) {
            cmd_end++;
        }
        
        // Set up input/output file descriptors
        if (cmd_end < tokens->length) { // Not last command
            out_idx = pipes[(2*i) + 1];
            if (out_idx == -1) {
                perror("pipe");
                return -1;
            }   
            
        } else { // Last command
            out_idx = STDOUT_FILENO;
        }
        if (i > 0) { // Not first command
            in_idx = pipes[2*(i - 1)];
            if (in_idx == -1) {
                perror("pipe");
                return -1;
            }  
        } else { // First command
            in_idx = STDIN_FILENO;
        }
        
        // Fork and execute command
        pid_t pid = fork();
        if (pid < 0) {
            perror("fork");
            return -1;
        } else if (pid == 0) { // Child process
            // Execute command
            strvec_t cmd_tokens;
            strvec_init(&cmd_tokens);
            for (int i = cmd_start; i < cmd_end; i++) {
                strvec_add(&cmd_tokens, tokens->data[i]);
            }
            // Call run_piped_command
            if (run_piped_command(&cmd_tokens, pipes, num_pipes, in_idx, out_idx) == -1) {
                perror("run piped command");
                return -1;
            }

            strvec_clear(&cmd_tokens);
            exit(EXIT_SUCCESS);

        } else { // Parent process
            // Close unused pipe file descriptors
            if (i > 0) {
                if (close(pipes[2*(i - 1)]) == -1) {
                    perror("close");
                    return -1;
                }
            }
            if (cmd_end < tokens->length) {
                if (close(pipes[(2*i) + 1]) == -1) {
                    perror("close");
                    return -1;
                }
            }
            // Move on to next command
            cmd_start = cmd_end + 1;
            i++;
        }
    }

    // Wait for child processes to finish
    int status;
    for (int i = 0; i < num_pipes + 1; i++) {
        wait(&status);
        if (WIFEXITED(status)) {
            if (WEXITSTATUS(status) != 0) {
                return -1;
            }
        } else {
            return -1;
        }
    }
    
    // Close all pipe file descriptors

    if (num_pipes == 1) {
        for (int i = 2; i < num_pipes * 2; i++) {
            if (close(pipes[i]) == -1) {
                printf("close %d", i);
                return -1;
            }
        }
    } else {
        for (int i = 4; i < num_pipes * 2; i++) {
            if (close(pipes[i]) == -1) {
                printf("close %d", i);
                return -1;
            }
        }
    }    

    return 0;
}