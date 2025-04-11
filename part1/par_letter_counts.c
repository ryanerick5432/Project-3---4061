#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#define ALPHABET_LEN 26

/*
 * Counts the number of occurrences of each letter (case insensitive) in a text
 * file and stores the results in an array.
 * file_name: The name of the text file in which to count letter occurrences
 * counts: An array of integers storing the number of occurrences of each letter.
 *     counts[0] is the number of 'a' or 'A' characters, counts [1] is the number
 *     of 'b' or 'B' characters, and so on.
 * Returns 0 on success or -1 on error.
 */
int count_letters(const char *file_name, int *counts) {
    // open file
    FILE *fp = fopen(file_name, "r");
    if (fp == NULL) {
        perror("fopen");
        return -1;
    }
    // create temporary var to hold chars - set as nonalphabet to prevent extra counting
    char temp_char = '1';
    // read one char
    while (fread(&temp_char, sizeof(char), 1, fp) > 0) {
        if (isalpha(temp_char)) {
            // ignore capitalizations
            temp_char = tolower(temp_char);
            // get ASCII value
            int index = temp_char;
            // First index, 0 = 'a' & 'a' = 97 - get index by (ASCII - 97)
            index = (index - 97);
            counts[index]++;
        }
    }
    if (fclose(fp) != 0) {
        perror("fclose");
        return -1;
    }
    return 0;
}

/*
 * Processes a particular file(counting occurrences of each letter)
 *     and writes the results to a file descriptor.
 * This function should be called in child processes.
 * file_name: The name of the file to analyze.
 * out_fd: The file descriptor to which results are written
 * Returns 0 on success or -1 on error
 */
int process_file(const char *file_name, int out_fd) {
    // initialize memory for alphabet num array
    int *counts = malloc(sizeof(int) * ALPHABET_LEN);
    if (counts == NULL) {
        return -1;
    }
    // initialize array w/0s
    for (int i = 0; i < ALPHABET_LEN; i++) {
        counts[i] = 0;
    }
    if (count_letters(file_name, counts) == -1) {
        free(counts);
        return -1;
    }
    // store write result in a variable to error check
    // write whole array at once
    ssize_t temp = write(out_fd, counts, sizeof(int) * ALPHABET_LEN);
    if (temp == -1) {
        perror("write");
        free(counts);
        return -1;
    }
    // free memory for array
    free(counts);
    return 0;
}

int main(int argc, char **argv) {
    if (argc == 1) {
        // No files to consume, return immediately
        return 0;
    }
    // TODO Create a pipe for child processes to write their results
    int pipe_fds[2];
    if (pipe(pipe_fds) == -1) {
        perror("pipe");
        return 1;
    }
    // TODO Fork a child to analyze each specified file (names are argv[1], argv[2], ...)
    // for loop for number of args - one child per arg/file
    for (int i = 1; i < argc; i++) {
        pid_t child_pid = fork();
        if (child_pid == -1) {
            perror("fork");
            return 1;
        }
        // child process
        if (child_pid == 0) {
            // close read end of pipe
            if (close(pipe_fds[0]) == -1) {
                perror("close");
                // exit 1 if error
                exit(1);
            }
            // process file
            if (process_file(argv[i], pipe_fds[1]) == -1) {
                exit(1);
            }
            exit(0);
        }
    }
    // parent process
    // close write end of pipe
    if (close(pipe_fds[1]) == -1) {
        perror("close");
        return 1;
    }
    // TODO Aggregate all the results together by reading from the pipe in the parent
    // initialize total val array
    int total_vals[ALPHABET_LEN];
    // initialize vals to 0
    for (int i = 0; i < ALPHABET_LEN; i++) {
        total_vals[i] = 0;
    }
    // iterate/read from every different child/pipe write
    for (int i = 0; i < (argc - 1); i++) {
        // use a temp val to add to total
        int temp_vals[ALPHABET_LEN];
        // initialize temp vals to 0
        for (int i = 0; i < ALPHABET_LEN; i++) {
            temp_vals[i] = 0;
        }
        // ream each write, one full array at a time and store in temp
        if (read(pipe_fds[0], &temp_vals, sizeof(int) * ALPHABET_LEN) == -1) {
            perror("read");
            return 1;
        }
        // iterate through that, adding the temp_vals to total (adds 0 if nothing read)
        for (int j = 0; j < ALPHABET_LEN; j++) {
            total_vals[j] = total_vals[j] + temp_vals[j];
        }
    }
    // done with read/close it
    if (close(pipe_fds[0]) == -1) {
        perror("close");
        return 1;
    }
    // extra check to wait for each child
    for (int i = 0; i < (argc - 1); i++) {
        int status;
        if (wait(&status) == -1) {
            return 1;
        }
        if (!WIFEXITED(status)) {
            return 1;
        }
    }
    // TODO Change this code to print out the total count of each letter (case insensitive)
    // prints each val out
    for (int i = 0; i < ALPHABET_LEN; i++) {
        printf("%c Count: %d\n", 'a' + i, total_vals[i]);
    }

    return 0;
}
