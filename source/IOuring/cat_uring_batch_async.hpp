#ifndef CAT_URING_BATCH_ASYNC_H
#define CAT_URING_BATCH_ASYNC_H

#include "./cat_uring_batch.hpp"

namespace uring_batch_async_impl
{
    // Handoff structure for io_uring user_data
    struct file_info
    {
        std::vector<struct iovec> iovecs;
    };

    ssize_t submit_to_sq(char *const file_path, struct submitter *s)
    {
        struct app_io_sq_ring *sring = &s->sq_ring;

        // Space check for 2 SQEs (Open + Read)
        if (uring_batch_impl::get_sq_occupancy(s) > QUEUE_DEPTH - 2)
            return -1;

        auto fi = std::make_unique<file_info>();
        const size_t ASYNC_BUF_SIZE = 1024 * 1024; // 1MB Buffer
        fi->iovecs.resize(1);

        if (posix_memalign(&fi->iovecs[0].iov_base, BLOCK_SZ, ASYNC_BUF_SIZE) != 0)
            return -1;
        fi->iovecs[0].iov_len = ASYNC_BUF_SIZE;

        unsigned tail = *sring->tail;
        unsigned open_idx = tail & *s->sq_ring.ring_mask;
        unsigned read_idx = (tail + 1) & *s->sq_ring.ring_mask;

        // 1. OPENAT SQE
        struct io_uring_sqe *sqe_open = &s->sqes[open_idx];
        memset(sqe_open, 0, sizeof(*sqe_open));
        sqe_open->opcode = IORING_OP_OPENAT;
        sqe_open->fd = AT_FDCWD;
        sqe_open->addr = reinterpret_cast<uintptr_t>(file_path);
        sqe_open->open_flags = O_RDONLY;
        sqe_open->flags = IOSQE_IO_LINK;
        sqe_open->user_data = 0; // Identifies Open operation

        // 2. READV SQE
        struct io_uring_sqe *sqe_read = &s->sqes[read_idx];
        memset(sqe_read, 0, sizeof(*sqe_read));
        sqe_read->opcode = IORING_OP_READV;
        sqe_read->fd = -1; // Linked to previous openat
        sqe_read->addr = reinterpret_cast<uintptr_t>(fi->iovecs.data());
        sqe_read->len = 1;
        sqe_read->off = 0;
        sqe_read->user_data = reinterpret_cast<uintptr_t>(fi.release());

        write_barrier();
        *sring->tail = tail + 2;
        return 0;
    }

    // Move this inside the function or reset it to 0 at the start of cat()
    size_t total_bytes_reaped = 0;

    int read_from_cq(struct submitter *s, bool quiet = false)
    {
        struct app_io_cq_ring *cring = &s->cq_ring;
        unsigned head = *cring->head;
        int completed_files = 0;

        while (head != *cring->tail)
        {
            read_barrier();
            struct io_uring_cqe *cqe = &cring->cqes[head & *cring->ring_mask];

            if (cqe->user_data == 0)
            {
                // OPENAT Result
                if (cqe->res < 0)
                {
                    std::println(stderr, "Async Open Error: {}", strerror(abs(cqe->res)));
                }
            }
            else
            {
                // READV Result
                std::unique_ptr<file_info> fi(reinterpret_cast<file_info *>(static_cast<uintptr_t>(cqe->user_data)));

                if (cqe->res > 0)
                {
                    total_bytes_reaped += cqe->res;
                    if (!quiet)
                    {
                        for (auto const &iov : fi->iovecs)
                            output_to_console(static_cast<char *>(iov.iov_base), cqe->res);
                    }
                }
                else if (cqe->res < 0 && cqe->res != -ECANCELED)
                {
                    std::println(stderr, "Async Read Error: {}", strerror(abs(cqe->res)));
                }

                // One file's lifecycle (Open + Read) is finished
                completed_files++;
            }
            head++;
        }

        write_barrier();
        *cring->head = head;
        return completed_files;
    }

    int uring_batch_async_cat(const int argc, char *const argv[])
    {
        bool quiet = false;
        total_bytes_reaped = 0; // RESET for benchmarking
        optind = 1;

        int opt;

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

        if (optind >= argc)
            return 1;

        auto s = std::make_unique<submitter>();
        if (app_setup_uring(s.get()))
            return 1;

        int files_to_process = 0;
        int files_completed = 0;
        auto start = std::chrono::high_resolution_clock::now();

        // Pass 1: Submission
        for (int i = optind; i < argc; i++)
        {
            // FIXED: Submit if we don't have room for 2 more SQE
            while (uring_batch_impl::get_sq_occupancy(s.get()) >= QUEUE_DEPTH - 1)
            {
                // FIX: Pass occupancy (to_submit) so the kernel actually sees the tail moved
                unsigned to_submit = uring_batch_impl::get_sq_occupancy(s.get());
                io_uring_enter(s->ring_fd, to_submit, 1, IORING_ENTER_GETEVENTS);
                files_completed += read_from_cq(s.get(), quiet);
            }

            if (submit_to_sq(argv[i], s.get()) == 0)
            {
                files_to_process++;
            }
        }

        // Final Submission
        unsigned final_sq = uring_batch_impl::get_sq_occupancy(s.get());
        if (final_sq > 0)
        {
            io_uring_enter(s->ring_fd, final_sq, 1, IORING_ENTER_GETEVENTS);
        }

        // Pass 2: Final Reaping
        while (files_completed < files_to_process)
        {
            files_completed += read_from_cq(s.get(), quiet);
            if (files_completed < files_to_process)
            {
                io_uring_enter(s->ring_fd, 0, 1, IORING_ENTER_GETEVENTS);
            }
        }

        auto end = std::chrono::high_resolution_clock::now();
        double ms = std::chrono::duration<double, std::milli>(end - start).count();
        double throughput = (total_bytes_reaped / (1024.0 * 1024.0)) / (ms / 1000.0);

        std::println("\nThroughput: {:>10.2f} MiB/s", throughput);
        return 0;
    }
}
#endif