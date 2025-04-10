#include "swish_funcs.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "string_vector.h"

#define MAX_ARGS 10

/*
 * Helper function to run a single command within a pipeline. You should make
 * make use of the provided 'run_command' function here.
 * tokens: String vector containing the tokens representing the command to be
 * executed, possible redirection, and the command's arguments.
 * pipes: An array of pipe file descriptors.
 * n_pipes: Length of the 'pipes' array
 * in_idx: Index of the file descriptor in the array from which the program
 *         should read its input, or -1 if input should not be read from a pipe.
 * out_idx: Index of the file descriptor in the array to which the program
 *          should write its output, or -1 if output should not be written to
 *          a pipe.
 * Returns 0 on success or -1 on error.
 */
int run_piped_command(strvec_t *tokens, int *pipes, int n_pipes, int in_idx, int out_idx) {
    // setup return value for error tracking
    int ret_val = 0;
    // loop through and close unnecessary pipes - i.e. pipes that are not in_idx, or out_idx.
    // error check to ensure closure occurs correctly
    for (int i = 0; i < n_pipes; i++) {
        if ((i == in_idx) || (i == out_idx)) {
            continue;
        } else {
            if (close(pipes[i]) == -1) {
                ret_val = -1;
            }
        }
    }
    // if error occured on closure, close the remaining two pipes and return error.
    if (ret_val == -1) {
        if (in_idx != -1) {
            close(pipes[in_idx]);
        }
        if (out_idx != -1) {
            close(pipes[out_idx]);
        }

        return ret_val;
    }

    // set up backup for standard input, in case of error
    int stdin_bak;
    // checks if input should be redirected, and redirects if so
    // error checks redirection, and cleans up if necessary
    if (in_idx != -1) {
        // sets up the backup for stdin, for error
        if ((stdin_bak = dup(STDIN_FILENO)) == -1) {
            close(pipes[in_idx]);
            if (out_idx != -1) {
                close(pipes[out_idx]);
            }
            return -1;
        }
        // redirects stdin to the input pipe.
        if (dup2(pipes[in_idx], STDIN_FILENO) == -1) {
            close(pipes[in_idx]);
            if (out_idx != -1) {
                close(pipes[out_idx]);
            }
            return -1;
        }
    }
    // set up backup for standard output, in case of error on exec call
    int stdout_bak;
    // redirects output if necessary.
    // error checks redirection, and cleans up if necessary
    if (out_idx != -1) {
        // sets up output backup
        if ((stdout_bak = dup(STDOUT_FILENO)) == -1) {
            close(pipes[out_idx]);
            if (in_idx != -1) {
                close(pipes[in_idx]);
                dup2(stdin_bak, STDIN_FILENO);
            }
            return -1;
        }
        // redirects output to pipe write end, from standard output.
        if (dup2(pipes[out_idx], STDOUT_FILENO) == -1) {
            close(pipes[out_idx]);
            if (in_idx != -1) {
                dup2(stdin_bak, STDIN_FILENO);
                close(pipes[in_idx]);
            }
            return -1;
        }
    }

    if (run_command(tokens) == -1) {
        // restores output and cleans if exec call does not occur
        if (in_idx != -1) {
            dup2(stdin_bak, STDIN_FILENO);
            close(pipes[in_idx]);
        }
        if (out_idx != -1) {
            dup2(stdout_bak, STDOUT_FILENO);
            close(pipes[out_idx]);
        }
        return -1;
    }
    return 0;
}

int run_pipelined_commands(strvec_t *tokens) {
    // total number of pipes will equal total of delimiting character '|'
    int num_pipe;
    if ((num_pipe = strvec_num_occurrences(tokens, "|")) < 1) {
        return -1;
    }
    // total number of processes is always one more than total pipes
    int num_proc = num_pipe + 1;

    // set up the commands token array, and tracker for delimiters
    strvec_t piped_commands[num_proc];
    int pipe_loc;

    // error index variable, to know how many tokens to clear on error.
    // -1 as any positive value is a theoretically valid index to error on
    int cleanup_idx = -1;
    // loop through, and partition each token in reverse order
    for (int i = num_proc - 1; i >= 0; i--) {
        // find the first token to split off
        if ((pipe_loc = strvec_find_last(tokens, "|")) == -1) {
            // if no pipes are found before end of tokens is reached, error occurs, as tokens have
            // fragmented
            if (i != 0) {
                cleanup_idx = i;
                // if reached end of tokens, slice remainer of tokens
            } else if (strvec_slice(tokens, &piped_commands[0], 0, tokens->length) == -1) {
                cleanup_idx = i;
            }
            // otherwise, continue slicing from end, from most recent pipe, to current end of tokens
        } else {
            if ((strvec_slice(tokens, &piped_commands[i], pipe_loc + 1, tokens->length)) == -1) {
                cleanup_idx = i;
                break;
            }
            // after slicing from end, cut off sliced portion, and partition character '|', and
            // continue
            strvec_take(tokens, pipe_loc);
        }
    }

    // if an error occurred in pipe-token partitioning, clean up all tokens, and exit on error
    if (cleanup_idx != -1) {
        for (int j = cleanup_idx; j < num_proc; j++) {
            strvec_clear(&piped_commands[j]);
        }
        return -1;
    }

    // creation of all pipes to be used (n-1 pipes total)
    int pipe_fds[num_pipe * 2];
    for (int i = 0; i < num_pipe; i++) {
        if (pipe(pipe_fds + 2 * i) == -1) {
            for (int j = 0; j < i; j++) {    // close all pipes on error
                close(pipe_fds[2 * j]);
                close(pipe_fds[2 * j + 1]);
            }
            for (int j = 0; j < num_proc; j++) {    // clear the tokens on error
                strvec_clear(&piped_commands[j]);
            }
            return -1;
        }
    }
    // handling each subprocess in pipe
    for (int i = 0; i < num_proc; i++) {
        pid_t c_pid = fork();

        if (c_pid == -1) {    // error on fork
            for (int j = 0; j < num_proc; j++) {
                close(pipe_fds[j]);
            }
            return -1;
        }
        // to execute in child process - each subprocess in pipe
        if (c_pid == 0) {
            if (i == 0) {
                // logic for process # 1 --- -1 = standard input, 1 = 1st pipe write end as output
                if (run_piped_command(&piped_commands[i], pipe_fds, num_pipe * 2, -1, 1) == -1) {
                    exit(1);
                }
                // logic for process # n --- 2 * i - 2 = input file, -1 = standard output
            } else if (i == num_pipe) {
                if (run_piped_command(&piped_commands[i], pipe_fds, num_pipe * 2, 2 * i - 2, -1) ==
                    -1) {
                    exit(1);
                }
                // logic for process # 2 .... i = n --- 1 2 * i - 2 = input file, 2 * i + 1 = output
                // file
            } else {
                if (run_piped_command(&piped_commands[i], pipe_fds, num_pipe * 2, 2 * i - 2,
                                      2 * i + 1) == -1) {
                    exit(1);
                }
            }
            exit(0);    // here for robust-ness, should never reach
        }
    }

    // return value - for errors
    int ret = 0;
    // clear all tokens, unneeded anymore
    for (int j = 0; j < num_proc; j++) {
        strvec_clear(&(piped_commands[j]));
    }
    // clear all pipes, necessary to remove blocking
    for (int j = 0; j < num_pipe * 2; j++) {
        if (close(pipe_fds[j]) == -1) {
            ret = -1;
        }
    }
    // exit status of children
    int status;
    // block for children to ensure processing order and arrival
    for (int i = 0; i < num_proc; i++) {
        // wait for all children
        if (wait(&status) == -1) {
            // if error on wait, clean up
            for (int j = 0; j < num_pipe; j++) {
                close(pipe_fds[j * 2]);
                close(pipe_fds[j * 2 + 1]);
            }
            for (int j = 0; j < num_proc; j++) {
                strvec_clear(&piped_commands[j]);
            }
            return -1;
        }
        // if any child exited on error (code 1), return -1
        if (WEXITSTATUS(status) != 0) {
            ret = -1;
        }
    }
    // return based on success - 0, or error - -1
    return ret;
}
