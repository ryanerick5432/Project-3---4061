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
    FILE *fp = fopen(file_name, "r");
    if (fp == NULL) {
        perror("fopen");
        return -1;
    }
    char temp_char = '1';
    while (fread(&temp_char, sizeof(char), 1, fp) > 0) {
        if (isalpha(temp_char)) {
            temp_char = tolower(temp_char);
            int index = temp_char;
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
    int *counts = malloc(sizeof(int) * ALPHABET_LEN);
    if (counts == NULL) {
        return -1;
    }
    for (int i = 0; i < ALPHABET_LEN; i++) {
        counts[i] = 0;
    }
    if (count_letters(file_name, counts) == -1) {
        free(counts);
        return -1;
    }
    ssize_t temp = write(out_fd, counts, sizeof(int) * ALPHABET_LEN);
    if (temp == -1) {
        perror("write");
        free(counts);
        return -1;
    }
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
    for (int i = 1; i < argc; i++) {
        pid_t child_pid = fork();
        if (child_pid == -1) {
            perror("fork");
            return 1;
        }
        if (child_pid == 0) {
            if (close(pipe_fds[0]) == -1) {
                perror("close");
                exit(1);
            }
            if (process_file(argv[i], pipe_fds[1]) == -1) {
                exit(1);
            }
            exit(0);
        }
    }
    if (close(pipe_fds[1]) == -1) {
        perror("close");
        return 1;
    }
    // TODO Aggregate all the results together by reading from the pipe in the parent
    int total_vals[ALPHABET_LEN];
    for (int i = 0; i < ALPHABET_LEN; i++) {
        total_vals[i] = 0;
    }
    for (int i = 0; i < (argc - 1); i++) {
        int temp_vals[ALPHABET_LEN];
        if (read(pipe_fds[0], &temp_vals, sizeof(int) * ALPHABET_LEN) == -1) {
            perror("read");
            return 1;
        }
        for (int j = 0; j < ALPHABET_LEN; j++) {
            total_vals[j] = total_vals[j] + temp_vals[j];
        }
    }
    if (close(pipe_fds[0]) == -1) {
        perror("close");
        return 1;
    }
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
    for (int i = 0; i < ALPHABET_LEN; i++) {
        printf("%c Count: %d\n", 'a' + i, total_vals[i]);
    }
    return 0;
}
