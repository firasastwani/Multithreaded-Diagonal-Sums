#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include "proj4.h"

// Structure to pass data to thread functions
typedef struct {

    grid *input;
    grid *output;
    unsigned long target_sum;
    int start_row;
    int end_row;
    int thread_id;

} thread_data;

// Function to check if a diagonal sum equals the target sum
/*
static int check_diagonal_sum(grid *g, int row, int col, int length, unsigned long target_sum)
{
    unsigned long sum = 0;
    for (int i = 0; i < length; i++)
    {
        sum += g->p[row + i][col + i];
    }
    return sum == target_sum;
}
*/

/**
 * Thread function that processes a portion of the grid to find diagonal sums.
 * Searches for both left-to-right and right-to-left diagonals.
 *
 * @param arg Pointer to thread_data structure containing thread's parameters
 * @return NULL
 * @assumptions Input grid contains valid digits (1-9)
 *              Thread data contains valid row ranges
 */
void *process_grid_portion(void *arg) {

    thread_data *data = (thread_data *)arg;

    grid *input = data->input;
    grid *output = data->output;

    int n = input->n;
    unsigned long target = data->target_sum;

    // Process assigned rows
    for (int i = data->start_row; i < data->end_row; i++)
    {

        for (int j = 0; j < n; j++)
        {
            // Left to right diagonal
            unsigned long sum = 0;
            int max_length = (n - i) < (n - j) ? (n - i) : (n - j);
            // Calculate maximum possible length that could sum to target
            int max_possible_length = target / 1; // Since minimum digit is 1

            if (max_possible_length > max_length) {
                max_possible_length = max_length;
            }

            for (int length = 1; length <= max_possible_length; length++) {

                sum += input->p[i + length - 1][j + length - 1];

                if (sum == target) {

                    for (int k = 0; k < length; k++) {

                        output->p[i + k][j + k] = input->p[i + k][j + k];
                    }
                }
                else if (sum > target) {

                    break; // No need to check longer lengths
                }
            }

            // Right to left diagonal
            if (j > 0) { // Only check if not in first column

                sum = 0;
                max_length = (n - i) < (j + 1) ? (n - i) : (j + 1);
                max_possible_length = target / 1;

                if (max_possible_length > max_length) {

                    max_possible_length = max_length;
                }

                for (int length = 1; length <= max_possible_length; length++) {

                    sum += input->p[i + length - 1][j - length + 1];

                    if (sum == target) {

                        for (int k = 0; k < length; k++) {

                            output->p[i + k][j - k] = input->p[i + k][j - k];
                        }
                    }
                    else if (sum > target) {

                        break; // No need to check longer lengths
                    }
                }
            }
        }
    }

    return NULL;
}

void initializeGrid(grid *g, char *fileName) {

    int fd = open(fileName, O_RDONLY);

    if (fd == -1) {
        perror("Error opening file");
        exit(1);
    }

    // First pass: count number of rows to determine n
    int n = 0;
    char ch;
    char buffer[1];

    while (read(fd, buffer, 1) > 0) {
        if (buffer[0] == '\n')
            n++;
    }
    lseek(fd, 0, SEEK_SET);

    // Allocate memory for grid
    g->n = n;
    g->p = (unsigned char **)malloc(n * sizeof(unsigned char *));

    for (int i = 0; i < n; i++) {
        g->p[i] = (unsigned char *)malloc(n * sizeof(unsigned char));
    }

    // Second pass: read grid values
    char line[n + 1]; // +1 for null terminator

    for (int i = 0; i < n; i++) {
        int bytes_read = 0;

        while (bytes_read < n) {
            int result = read(fd, line + bytes_read, n - bytes_read);

            if (result == -1) {
                perror("Error reading file");
                exit(1);
            }

            bytes_read += result;
        }
        // Skip newline
        read(fd, buffer, 1);

        for (int j = 0; j < n; j++) {
            g->p[i][j] = line[j] - '0';
        }
    }
    close(fd);
}

void diagonalSums(grid *input, unsigned long s, grid *output, int t) {
    int n = input->n;

    // Initialize output grid
    output->n = n;
    output->p = (unsigned char **)malloc(n * sizeof(unsigned char *));

    for (int i = 0; i < n; i++) {
        output->p[i] = (unsigned char *)malloc(n * sizeof(unsigned char));
        memset(output->p[i], 0, n * sizeof(unsigned char));
    }

    if (t == 1) 
    {
        // Single thread case
        thread_data data = {input, output, s, 0, n, 0};
        process_grid_portion(&data);
    } else {
        // Multi-thread case
        pthread_t threads[t - 1];
        thread_data thread_args[t];

        // Calculate rows per thread
        int rows_per_thread = n / t;
        int remaining_rows = n % t;

        // Create threads
        int current_row = 0;

        for (int i = 0; i < t; i++) {
            thread_args[i].input = input;
            thread_args[i].output = output;
            thread_args[i].target_sum = s;
            thread_args[i].start_row = current_row;
            thread_args[i].end_row = current_row + rows_per_thread + (i < remaining_rows ? 1 : 0);
            thread_args[i].thread_id = i;

            if (i < t - 1) 
            {
                pthread_create(&threads[i], NULL, process_grid_portion, &thread_args[i]);
            }

            else {
                process_grid_portion(&thread_args[i]);
            }

            current_row = thread_args[i].end_row;
        }

        // Join threads
        for (int i = 0; i < t - 1; i++) {
            pthread_join(threads[i], NULL);
        }
    }
}

void writeGrid(grid *g, char *fileName) 
{
    int fd = open(fileName, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);

    if (fd == -1) {
        perror("Error opening file");
        exit(1);
    }

    // Write grid to file
    char buffer[g->n + 1]; // +1 for newline

    for (int i = 0; i < g->n; i++) {

        for (int j = 0; j < g->n; j++) {
            buffer[j] = g->p[i][j] + '0';
        }
        buffer[g->n] = '\n';


        if (write(fd, buffer, g->n + 1) != g->n + 1) {
            perror("Error writing to file");
            exit(1);
        }
    }
    close(fd);
}

void freeGrid(grid *g) {

    if (g->p) {
        
        for (int i = 0; i < g->n; i++) {
            free(g->p[i]);
        }
        free(g->p);
    }
}
