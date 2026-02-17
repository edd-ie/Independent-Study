//
// Created by _edd.ie_ on 16/02/2026.
//

#ifndef CAT_READV_H
#define CAT_READV_H

#include <stdio.h>
#include <sys/uio.h>
#include <sys/stat.h>
#include <linux/fs.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <stdlib.h>
#include <vector>
#include <chrono>
#include <print>
#include <unistd.h>

/*
 * ******* Task ****************
 * The cat command concatenates & prints the contents of files
 * that are passed in as arguments to the command
 *
 * ******* Action **************
 * 1. Read the file chunk by chunk
 * 2. Point to each of those chunks with an iovec structure.
 * 3. readv() blocks and when it returns, if no errors,
 * 4. the iovec point to a set of buffers with the file data
 * 5. Print those to the console.
 */

#define CHUNK_SIZE 4096

/*
 * Each struct simply points to a buffer.
 * A base address and a length.
 */
// struct iovec
// {
//     void *iov_base; /* Starting address */
//     size_t iov_len; /* Number of bytes to transfer */
// };

/*
 * Returns the size of the file whose open file descriptor is passed in.
 * Properly handles regular file and block devices as well.
 */
off_t get_file_size(int fd)
{
    struct stat st;

    if (fstat(fd, &st) < 0)
    {
        perror("fstat");
        return -1;
    }

    if (S_ISBLK(st.st_mode))
    {
        unsigned long long bytes;

        if (ioctl(fd, BLKGETSIZE64, &bytes) != 0)
        {
            perror("ioctl");
            return -1;
        }
        return bytes;
    }
    else if (S_ISREG(st.st_mode))
        return st.st_size;
    return -1;
}

/*
 * Output a string of characters of len length to stdout.
 * Buffered output for efficient,
 * As it needs to output character-by-character.
 * */
void output_to_console(char *buf, int len)
{
    while (len--)
    {
        fputc(*buf++, stdout);
    }
}

ssize_t read_and_print_file(char *const file_name, bool quiet = false)
{
    // Reading the file
    int file_fd = open(file_name, O_RDONLY);
    if (file_fd < 0)
    {
        perror("Error opening the file");
        return -1;
    }

    // Getting the file size
    off_t file_sz = get_file_size(file_fd);
    off_t bytes_remaining = file_sz;

    // Chunking
    // allocate enough blocks to be able to hold the file data.
    // Each block is described in an iovec structure,
    // It'll be passed to readv as part of the array of iovecs.
    int blocks = (int)file_sz / CHUNK_SIZE;
    if (file_sz % CHUNK_SIZE)
        blocks++;
    std::vector<iovec> iovecs(blocks); // Point to data & length

    // memory allocation
    int current_block = 0;
    while (bytes_remaining)
    {
        off_t bytes_to_read = bytes_remaining;
        if (bytes_to_read > CHUNK_SIZE)
            bytes_to_read = CHUNK_SIZE;
        void *buf;

        if (posix_memalign(&buf, CHUNK_SIZE, CHUNK_SIZE))
        {
            perror("posix_memalign failed to allocate");
            return -1;
        }
        iovecs[current_block].iov_base = buf; // pointer to allocated memory
        iovecs[current_block].iov_len = bytes_to_read;
        current_block++;
        bytes_remaining -= bytes_to_read;
    }

    // The readv() call will block until all iovec buffers
    //  are filled with file data.
    //  Once it returns, you access the file data
    //  from the iovecs and print them on the console.
    int ret = readv(file_fd, iovecs.data(), blocks);
    if (ret < 0)
    {
        perror("readv failure");
        return -1;
    }

    // Only output if quiet mode is OFF
    if (!quiet)
    {
        for (int i = 0; i < blocks; i++)
            output_to_console(static_cast<char *>(iovecs[i].iov_base), iovecs[i].iov_len);
    }

    // Memory cleanup
    for (int i = 0; i < blocks; i++)
    {
        // Freeing the actual buffer, not the iovec structure
        free(iovecs[i].iov_base);
    }

    // The vector 'iovecs' will automatically clean up its own
    // internal array when the function returns.
    return static_cast<ssize_t>(file_sz);
}

int test_cat(const int argc, char *const argv[])
{
    bool quiet = false;
    int opt;

    // Reset getopt in case test_cat is called multiple times
    optind = 1;

    // Parse flags
    while ((opt = getopt(argc, argv, "q")) != -1)
    {
        if (opt == 'q')
        {
            quiet = true;
        }
        else
        {
            std::println(stderr, "Usage: {} [-q] <filename1> ...", argv[0]);
            return 1;
        }
    }

    // After getopt, optind is the index of the first non-option argument
    if (optind >= argc)
    {
        std::println(stderr, "Expected argument after options");
        return 1;
    }

    size_t total_bytes = 0;

    // Start timing
    auto start = std::chrono::high_resolution_clock::now();

    // For each file that is passed in as the argument,
    // call the read_and_print_file()
    for (int i = optind; i < argc; i++)
    {
        ssize_t bytes_read = read_and_print_file(argv[i], quiet);
        if (bytes_read < 0)
        {
            std::println(stderr, "Error reading file: {}", argv[i]);
            return 1;
        }
        total_bytes += bytes_read;
    }

    // End timing
    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> duration_sec = end - start;

    // Calculate Metrics
    double duration_ms = duration_sec.count() * 1000.0;
    double mib = total_bytes / (1024.0 * 1024.0);
    double throughput = mib / duration_sec.count();

    // Use Python-style formatting for a professional CLI look
    std::println("\n{:=>30}", "");                 // Prints a line of 30 '='
    std::println("{:^30}", "PERFORMANCE METRICS"); // Centered text
    std::println("{:=>30}", "");

    // Formatting the Output
    std::println("\n{:=>40}", "");
    std::println("{:^40}", "I/O PERFORMANCE REPORT");
    std::println("{:=>40}", "");
    std::println("Total Data:      {:>10.2f} MiB", mib);
    std::println("Total Time:      {:>10.3f} ms", duration_ms);
    std::println("Throughput:      {:>10.2f} MiB/s", throughput);
    std::println("{:=>40}", "");

    return 0;
}

#endif // CAT_READV_H