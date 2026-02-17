//
// Created by _edd.ie_ on 16/02/2026.
//

#ifndef CAT_URING_BATCH_H
#define CAT_URING_BATCH_H

/*
 * Place as many request as the queue length will allow.
 * These operations can be a mix of reads, writes, etc.
 * Then, call the io_uring_enter() system call to tell
 * the kernel requests are added to the submission queue.
 * Once the kernel's done, CQEs can be accessed from user space.
 */

#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/syscall.h>
#include <sys/mman.h>
#include <sys/uio.h>
#include <linux/fs.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <linux/io_uring.h>
#include <cstdint>
#include <memory>
#include <vector>
#include <print>
#include <algorithm>
#include <chrono>
#include "./cat_uring.hpp"

namespace uring_batch_impl
{
    /*
     * Read completion events from completion queue.
     * Get the data buffer that will have the file data
     * Print it to the console.
     * */
    int read_from_cq(struct submitter *s, bool quiet = false)
    {
        struct app_io_cq_ring *cring = &s->cq_ring;
        struct io_uring_cqe *cqe;
        unsigned head = *cring->head;
        int count = 0;

        while (true)
        {
            read_barrier();
            /*
             * This is a ring buffer.
             * If head == tail then:
             *  buffer is empty.
             * */
            if (head == *cring->tail)
                break;

            /* Get the entry */
            cqe = &cring->cqes[head & *cring->ring_mask];
            // Take the raw address from user_data and put it back into a unique_ptr.
            std::unique_ptr<file_info> fi(reinterpret_cast<file_info *>(static_cast<uintptr_t>(cqe->user_data)));

            if (cqe->res < 0)
                std::println(stderr, "Async I/O Error: {}", strerror(abs(cqe->res)));
            else if (!quiet)
            {

                int blocks = fi->iovecs.size();

                for (int i = 0; i < blocks; i++)
                    output_to_console(static_cast<char *>(fi->iovecs[i].iov_base), fi->iovecs[i].iov_len);
            }
            head++;
            count++;
        }
        write_barrier();
        *cring->head = head;
        return count;
    }

    /*
     * Submit to submission queue.
     * This function submits requests to the submission queue.
     * You can submit many types of requests.
     * This submits readv() request, specified via IORING_OP_READV.
     * */

    ssize_t submit_to_sq(char *const file_path, struct submitter *s)
    {
        int file_fd = open(file_path, O_RDONLY);
        if (file_fd < 0)
        {
            perror("Error: failed to open file for submission to SQ");
            return -1;
        }

        struct app_io_sq_ring *sring = &s->sq_ring;
        unsigned index = 0, current_block = 0, tail = 0, next_tail = 0;

        off_t file_sz = get_file_size(file_fd);
        if (file_sz < 0)
        {
            perror("Error: Getting file size for submission to SQ");
            return -1;
        }

        off_t bytes_remaining = file_sz;

        auto fi = std::make_unique<file_info>();
        if (!fi)
        {
            std::println(stderr, "Unable to allocate memory");
            return -1;
        }

        fi->file_sz = file_sz;
        fi->fd = file_fd;
        int blocks = fi->get_blocks();

        // Resizing up front, guaranteed a stable memory address for the duration of the I/O.
        fi->iovecs.resize(blocks);

        /*
         * For each block of the file to be read, we allocate an iovec struct
         * which is indexed into the iovecs array. This array is passed in as part
         * of the submission.
         * */
        while (bytes_remaining > 0)
        {
            // explicitly tell the compiler to use the std::min template with the type off_t.
            // forces conversion to off_t
            off_t bytes_to_read = std::min<off_t>(bytes_remaining, BLOCK_SZ);

            fi->iovecs[current_block].iov_len = bytes_to_read;

            void *buf = nullptr;
            if (posix_memalign(&buf, BLOCK_SZ, BLOCK_SZ))
            {
                perror("posix_memalign failed to allocate");
                return -1;
            }

            fi->iovecs[current_block].iov_base = buf; // pointer to allocated memory
            fi->iovecs[current_block].iov_len = bytes_to_read;

            current_block++;
            bytes_remaining -= bytes_to_read;
        }

        /* Add our submission queue entry to the tail of the SQE ring buffer */
        next_tail = tail = *sring->tail;
        next_tail++;
        read_barrier();
        index = tail & *s->sq_ring.ring_mask;
        struct io_uring_sqe *sqe = &s->sqes[index];
        sqe->fd = file_fd;
        sqe->flags = 0;
        sqe->opcode = IORING_OP_READV;

        // Hand off the raw pointer to the kernel via user_data
        sqe->addr = reinterpret_cast<uintptr_t>(fi->iovecs.data());
        sqe->len = blocks;
        sqe->off = 0;

        // .release() returns the pointer and gives up ownership
        sqe->user_data = reinterpret_cast<uintptr_t>(fi.release());
        sring->array[index] = index;
        tail = next_tail;

        /* Update the tail so the kernel can see it. */
        if (*sring->tail != tail)
        {
            /*
             * write_barrier() must happen before the tail is updated.
             * If you update the tail first,
             *   - The kernel might see the new tail
             *   - Try to read the SQE before the CPU has actually finished writing the SQE data to memory.
             */
            write_barrier();
            *sring->tail = tail;
        }

        /*
         * Tell the kernel submitted events with the io_uring_enter() system call.
         * Passing in the IOURING_ENTER_GETEVENTS flag which causes the
         * io_uring_enter() call to wait until min_complete events (the 3rd param)
         * complete.
         * */
        // int ret = io_uring_enter(s->ring_fd, 1, 1, IORING_ENTER_GETEVENTS);
        // if (ret < 0)
        // {
        //     perror("Error: Failed to subit events, io_uring_enter");
        //     return -1;
        // }

        return static_cast<ssize_t>(file_sz);
    }

    unsigned get_sq_occupancy(struct submitter *s)
    {
        // Current tail minus current head = number of entries the kernel hasn't processed yet
        return *s->sq_ring.tail - *s->sq_ring.head;
    }

    int uring_batch_cat(const int argc, char *const argv[])
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

        auto s = std::make_unique<submitter>();
        if (!s)
        {
            perror("Error creating struct submitter");
            return 1;
        }

        // No need for memset! std::make_unique<submitter>(), already zero-initializes the struct members.
        // memset(s, 0, sizeof(*s));

        // access the raw pointer inside a unique_ptr, use the .get()
        if (app_setup_uring(s.get()))
        {
            std::println(stderr, "Unable to setup uring!");
            return 1;
        }

        int files_to_process = 0;
        int files_completed = 0;
        size_t total_bytes = 0;

        // Start timing
        auto start = std::chrono::high_resolution_clock::now();

        // PASS 1: Submit everything
        for (int i = optind; i < argc; i++)
        {

            // --- SPACE CHECK ---
            // If the SQ is full, we MUST reap at least one completion to make space
            while (get_sq_occupancy(s.get()) >= QUEUE_DEPTH)
            {
                // Tell kernel to process what's there and wait for 1 completion
                io_uring_enter(s->ring_fd, get_sq_occupancy(s.get()), 1, IORING_ENTER_GETEVENTS);
                files_completed += read_from_cq(s.get(), quiet);
            }

            ssize_t bytes_read = submit_to_sq(argv[i], s.get());
            if (bytes_read < 0)
            {
                std::println(stderr, "Error reading file: {}", argv[i]);
                return 1;
            }
            total_bytes += bytes_read;
            files_to_process++;
        }

        // Send the last batch remaining in the SQ
        unsigned final_batch = get_sq_occupancy(s.get());
        if (final_batch > 0)
        {
            io_uring_enter(s->ring_fd, final_batch, 1, IORING_ENTER_GETEVENTS);
        }

        // PASS 2: Reap everything
        while (files_completed < files_to_process)
        {
            // This will now process all available CQEs in the ring
            files_completed += read_from_cq(s.get(), quiet);

            // If we haven't finished yet but the queue is empty,
            // wait for more events.
            if (files_completed < files_to_process)
            {
                io_uring_enter(s->ring_fd, 0, 1, IORING_ENTER_GETEVENTS);
            }
        }

        // End timing
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = end - start;

        // Calculate Metrics
        double ms = std::chrono::duration<double, std::milli>(duration).count();
        double seconds = ms / 1000.0;
        double mib = total_bytes / (1024.0 * 1024.0);
        double throughput = (seconds > 0) ? (mib / seconds) : 0;

        // Use Python-style formatting for a professional CLI look
        std::println("\n{:=>30}", "");                 // Prints a line of 30 '='
        std::println("{:^30}", "PERFORMANCE METRICS"); // Centered text
        std::println("{:=>30}", "");

        // Formatting the Output
        std::println("\n{:=>40}", "");
        std::println("{:^40}", "I/O PERFORMANCE REPORT");
        std::println("{:=>40}", "");
        std::println("Total Data:      {:>10.2f} MiB", mib);
        std::println("Total Time:      {:>10.3f} ms", ms);
        std::println("Throughput:      {:>10.2f} MiB/s", throughput);
        std::println("{:=>40}", "");

        return 0;
    }

}

#endif // CAT_URING_BATCH_H