#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include "proj4.h"

// Structure to pass data to thread functions
typedef struct
{
    grid *input;
    grid *output;
    unsigned long target_sum;
    int start_row;
    int end_row;
    int thread_id;
} thread_data;

// Cache line size for alignment
#define CACHE_LINE_SIZE 64
#define BLOCK_SIZE 128 // Increased block size for better cache utilization
#define CHUNK_SIZE 8   // For loop unrolling

// Helper function for aligned memory allocation
static void *aligned_malloc(size_t size)
{
    void *ptr = NULL;
    if (posix_memalign(&ptr, CACHE_LINE_SIZE, size) != 0)
    {
        return NULL;
    }
    return ptr;
}

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

// Process a block of the grid with optimized memory access
static void process_block(grid *input, grid *output, int start_i, int end_i,
                          int start_j, int end_j, unsigned long target)
{
    int n = input->n;
    int max_possible_length = target / 1;
    if (max_possible_length > n)
        max_possible_length = n;

    for (int i = start_i; i < end_i; i++)
    {
        // Pre-calculate row bounds
        int max_length_i = n - i;

        for (int j = start_j; j < end_j; j++)
        {
            // Pre-calculate column bounds
            int max_length_j = n - j;
            int max_length = max_length_i < max_length_j ? max_length_i : max_length_j;
            int actual_max_length = max_possible_length < max_length ? max_possible_length : max_length;

            // Left to right diagonal
            unsigned long sum = 0;
            int length;

            // Process in chunks of 4 for better cache utilization
            for (length = 1; length <= actual_max_length - 3; length += 4)
            {
                sum += input->p[i + length - 1][j + length - 1];
                if (sum == target)
                {
                    for (int k = 0; k < length; k++)
                    {
                        output->p[i + k][j + k] = input->p[i + k][j + k];
                    }
                    break;
                }
                if (sum > target)
                    break;

                sum += input->p[i + length][j + length];
                if (sum == target)
                {
                    for (int k = 0; k < length + 1; k++)
                    {
                        output->p[i + k][j + k] = input->p[i + k][j + k];
                    }
                    break;
                }
                if (sum > target)
                    break;

                sum += input->p[i + length + 1][j + length + 1];
                if (sum == target)
                {
                    for (int k = 0; k < length + 2; k++)
                    {
                        output->p[i + k][j + k] = input->p[i + k][j + k];
                    }
                    break;
                }
                if (sum > target)
                    break;

                sum += input->p[i + length + 2][j + length + 2];
                if (sum == target)
                {
                    for (int k = 0; k < length + 3; k++)
                    {
                        output->p[i + k][j + k] = input->p[i + k][j + k];
                    }
                    break;
                }
                if (sum > target)
                    break;
            }

            // Process remaining elements
            for (; length <= actual_max_length; length++)
            {
                sum += input->p[i + length - 1][j + length - 1];
                if (sum == target)
                {
                    for (int k = 0; k < length; k++)
                    {
                        output->p[i + k][j + k] = input->p[i + k][j + k];
                    }
                }
                else if (sum > target)
                    break;
            }

            // Right to left diagonal
            if (j > 0)
            {
                sum = 0;
                max_length = (n - i) < (j + 1) ? (n - i) : (j + 1);
                actual_max_length = max_possible_length < max_length ? max_possible_length : max_length;

                // Process in chunks of 4
                for (length = 1; length <= actual_max_length - 3; length += 4)
                {
                    sum += input->p[i + length - 1][j - length + 1];
                    if (sum == target)
                    {
                        for (int k = 0; k < length; k++)
                        {
                            output->p[i + k][j - k] = input->p[i + k][j - k];
                        }
                        break;
                    }
                    if (sum > target)
                        break;

                    sum += input->p[i + length][j - length];
                    if (sum == target)
                    {
                        for (int k = 0; k < length + 1; k++)
                        {
                            output->p[i + k][j - k] = input->p[i + k][j - k];
                        }
                        break;
                    }
                    if (sum > target)
                        break;

                    sum += input->p[i + length + 1][j - length - 1];
                    if (sum == target)
                    {
                        for (int k = 0; k < length + 2; k++)
                        {
                            output->p[i + k][j - k] = input->p[i + k][j - k];
                        }
                        break;
                    }
                    if (sum > target)
                        break;

                    sum += input->p[i + length + 2][j - length - 2];
                    if (sum == target)
                    {
                        for (int k = 0; k < length + 3; k++)
                        {
                            output->p[i + k][j - k] = input->p[i + k][j - k];
                        }
                        break;
                    }
                    if (sum > target)
                        break;
                }

                // Process remaining elements
                for (; length <= actual_max_length; length++)
                {
                    sum += input->p[i + length - 1][j - length + 1];
                    if (sum == target)
                    {
                        for (int k = 0; k < length; k++)
                        {
                            output->p[i + k][j - k] = input->p[i + k][j - k];
                        }
                    }
                    else if (sum > target)
                        break;
                }
            }
        }
    }
}

// Optimized thread function with block-based processing
void *process_grid_portion(void *arg)
{
    thread_data *data = (thread_data *)arg;
    grid *input = data->input;
    grid *output = data->output;
    int n = input->n;

    // Process in larger blocks for better cache utilization
    for (int block_i = data->start_row; block_i < data->end_row; block_i += BLOCK_SIZE)
    {
        int end_block_i = (block_i + BLOCK_SIZE < data->end_row) ? block_i + BLOCK_SIZE : data->end_row;

        for (int block_j = 0; block_j < n; block_j += BLOCK_SIZE)
        {
            int end_block_j = (block_j + BLOCK_SIZE < n) ? block_j + BLOCK_SIZE : n;

            process_block(input, output, block_i, end_block_i,
                          block_j, end_block_j, data->target_sum);
        }
    }
    return NULL;
}

// Optimized grid initialization using memory mapping
void initializeGrid(grid *g, char *fileName)
{
    int fd = open(fileName, O_RDONLY);
    if (fd == -1)
    {
        perror("Error opening file");
        exit(1);
    }

    struct stat st;
    if (fstat(fd, &st) == -1)
    {
        perror("Error getting file stats");
        close(fd);
        exit(1);
    }

    // Memory map the file
    char *file_content = mmap(NULL, st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    if (file_content == MAP_FAILED)
    {
        perror("Error memory mapping file");
        close(fd);
        exit(1);
    }

    // Count rows
    int n = 0;
    for (size_t i = 0; i < st.st_size; i++)
    {
        if (file_content[i] == '\n')
            n++;
    }

    // Allocate aligned memory for grid
    g->n = n;
    g->p = (unsigned char **)aligned_malloc(n * sizeof(unsigned char *));
    if (!g->p)
    {
        perror("Error allocating memory");
        munmap(file_content, st.st_size);
        close(fd);
        exit(1);
    }

    for (int i = 0; i < n; i++)
    {
        g->p[i] = (unsigned char *)aligned_malloc(n * sizeof(unsigned char));
        if (!g->p[i])
        {
            perror("Error allocating memory");
            for (int j = 0; j < i; j++)
                free(g->p[j]);
            free(g->p);
            munmap(file_content, st.st_size);
            close(fd);
            exit(1);
        }
    }

    // Parse grid values with optimized memory access
    size_t pos = 0;
    for (int i = 0; i < n; i++)
    {
        memcpy(g->p[i], file_content + pos, n);
        for (int j = 0; j < n; j++)
        {
            g->p[i][j] -= '0';
        }
        pos += n + 1; // Skip newline
    }

    munmap(file_content, st.st_size);
    close(fd);
}

// Optimized diagonal sums with better thread distribution
void diagonalSums(grid *input, unsigned long s, grid *output, int t)
{
    int n = input->n;

    // Initialize output grid with aligned memory
    output->n = n;
    output->p = (unsigned char **)aligned_malloc(n * sizeof(unsigned char *));
    if (!output->p)
    {
        perror("Error allocating memory");
        exit(1);
    }

    for (int i = 0; i < n; i++)
    {
        output->p[i] = (unsigned char *)aligned_malloc(n * sizeof(unsigned char));
        if (!output->p[i])
        {
            perror("Error allocating memory");
            for (int j = 0; j < i; j++)
                free(output->p[j]);
            free(output->p);
            exit(1);
        }
        memset(output->p[i], 0, n * sizeof(unsigned char));
    }

    if (t == 1)
    {
        thread_data data = {input, output, s, 0, n, 0};
        process_grid_portion(&data);
    }
    else
    {
        pthread_t threads[t - 1];
        thread_data thread_args[t];

        // Improved thread distribution with better load balancing
        int rows_per_thread = n / t;
        int remaining_rows = n % t;
        int current_row = 0;

        // Create threads with better workload distribution
        for (int i = 0; i < t; i++)
        {
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
            else
            {
                process_grid_portion(&thread_args[i]);
            }

            current_row = thread_args[i].end_row;
        }

        // Join threads
        for (int i = 0; i < t - 1; i++)
        {
            pthread_join(threads[i], NULL);
        }
    }
}

// Optimized grid writing with buffered I/O
void writeGrid(grid *g, char *fileName)
{
    FILE *fp = fopen(fileName, "w");
    if (!fp)
    {
        perror("Error opening file");
        exit(1);
    }

    // Use buffered I/O for better performance
    char *buffer = (char *)malloc(g->n + 1);
    if (!buffer)
    {
        perror("Error allocating memory");
        fclose(fp);
        exit(1);
    }

    for (int i = 0; i < g->n; i++)
    {
        for (int j = 0; j < g->n; j++)
        {
            buffer[j] = g->p[i][j] + '0';
        }
        buffer[g->n] = '\n';

        if (fwrite(buffer, 1, g->n + 1, fp) != g->n + 1)
        {
            perror("Error writing to file");
            free(buffer);
            fclose(fp);
            exit(1);
        }
    }

    free(buffer);
    fclose(fp);
}

// Optimized grid cleanup
void freeGrid(grid *g)
{
    if (g->p)
    {
        for (int i = 0; i < g->n; i++)
        {
            free(g->p[i]);
        }
        free(g->p);
    }
}
