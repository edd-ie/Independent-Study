#pragma once

#include <sys/stat.h>
#include <print>
#include <liburing.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/uio.h>
#include "Request.hpp"
#include <span>
#include <list>
#include <string.h>

namespace Network
{
    enum class OP_FLAGS
    {
        SRC_SPLICE_PIPE,
        PIPE_TEE_PIPE,
        PIPE_SPLICE_PIPE,
        PIPE_SPLICE_DEST
    };

    const unsigned int CHUNK_SIZE = 64 * 1024;

    // Returns 1 if successful
    size_t prepare_tee(io_uring &ring, IO_Handle &in, IO_Handle &out, size_t size, int i, bool link)
    {
        io_uring_sqe *sqe = io_uring_get_sqe(&ring);
        if (!sqe)
            return 0;

        io_uring_prep_tee(sqe, in.native_handle(), out.native_handle(), size, 0);

        if (link)
            sqe->flags |= IOSQE_IO_LINK;

        uintptr_t packed = (static_cast<uintptr_t>(OP_FLAGS::PIPE_TEE_PIPE) << 32) | (i & 0xFFFFFFFF);
        io_uring_sqe_set_data(sqe, reinterpret_cast<void *>(packed));
        return 1;
    }

    size_t prepare_splice(io_uring &ring, IO_Handle &in, IO_Handle &out, OP_FLAGS flag, off_t offset, size_t size, int i, bool link = false)
    {
        io_uring_sqe *sqe = io_uring_get_sqe(&ring);
        if (!sqe)
            return 0;

        int64_t off_in = (flag == OP_FLAGS::SRC_SPLICE_PIPE) ? (int64_t)offset : -1;
        int64_t off_out = (flag == OP_FLAGS::PIPE_SPLICE_DEST) ? (int64_t)offset : -1;

        io_uring_prep_splice(sqe, in.native_handle(), off_in, out.native_handle(), off_out, size, SPLICE_F_MOVE);

        if (link)
            sqe->flags |= IOSQE_IO_LINK;

        uintptr_t packed = (static_cast<uintptr_t>(flag) << 32) | (i & 0xFFFFFFFF);
        io_uring_sqe_set_data(sqe, reinterpret_cast<void *>(packed));
        return 1;
    }

    size_t process_cqe(io_uring &ring)
    {
        io_uring_cqe *cqe;
        unsigned head;
        size_t total_data = 0;
        size_t completed_cqes = 0;
        io_uring_for_each_cqe(&ring, head, cqe)
        {
            if (cqe->res == -ECANCELED || cqe->res == -EAGAIN)
                continue;

            if (cqe->res < 0)
            {
                std::println(stderr, "Error: {}", strerror(-cqe->res));
                return 0;
            }

            uintptr_t packed = reinterpret_cast<uintptr_t>(io_uring_cqe_get_data(cqe));
            total_data += packed & 0xFFFFFFFF;

            completed_cqes++;
        }
        io_uring_cq_advance(&ring, completed_cqes);
        return total_data;
    }

    size_t perform_tee_broadcast(IO_Handle &source_file, IO_Handle &src_write_pipe, IO_Handle &src_read_pipe, std::span<IO_Handle> &output_files, std::span<IO_Handle> dest_write_pipes, std::span<IO_Handle> dest_read_pipes)
    {
        io_uring ring{};
        if (io_uring_queue_init(128, &ring, 0) < 0)
        {
            std::println(stderr, "Failure to init queue!");
            exit(EIO);
        }

        // uint64_t src_offset = 0;
        // std::vector<uint64_t> dest_offsets(output_files.size(), 0);

        size_t data_size = 0;
        size_t file_sz = IO_Handle::get_file_size(source_file);
        size_t total_processed = 0;

        while (total_processed < file_sz)
        {
            size_t to_process = std::min<size_t>(CHUNK_SIZE, file_sz - total_processed);

            // 1. READ: File -> Master Pipe (NO LINK - avoid fragile chain)
            prepare_splice(ring, source_file, src_write_pipe, OP_FLAGS::SRC_SPLICE_PIPE, total_processed, to_process, 999);

            int expected_cqes = 1;

            // 2. TEE & SPLICE: For all but the last file
            for (size_t i = 0; i < output_files.size() - 1; ++i)
            {
                // Link the TEE to its OWN SPLICE only
                prepare_tee(ring, src_read_pipe, dest_write_pipes[i], to_process, i, true);
                prepare_splice(ring, dest_read_pipes[i], output_files[i],
                               OP_FLAGS::PIPE_SPLICE_DEST, total_processed, to_process, i, false);
                expected_cqes += 2;
            }

            // 3. CONSUME: The last file clears the master pipe
            size_t last = output_files.size() - 1;
            prepare_splice(ring, src_read_pipe, output_files[last],
                           OP_FLAGS::PIPE_SPLICE_DEST, total_processed, to_process, last, false);
            expected_cqes += 1;

            io_uring_submit(&ring);

            // 4. WAIT: Block and handle short splices
            int completed = 0;
            while (completed < expected_cqes)
            {
                io_uring_cqe *cqe;
                if (io_uring_wait_cqe(&ring, &cqe) == 0)
                {
                    if (cqe->res < 0)
                    {
                        // If you still see ECANCELED here, it means the TEE failed (likely pipe full)
                        std::println(stderr, "Op Error: {}", strerror(-cqe->res));
                    }
                    else if ((size_t)cqe->res < to_process)
                    {
                        // To tell it like it is: This is where your data corruption starts.
                        // A real-world app would need to resubmit the remaining bytes here.
                    }
                    io_uring_cqe_seen(&ring, cqe);
                    completed++;
                }
            }
            total_processed += to_process;
        }

        io_uring_queue_exit(&ring);
        return data_size;
    }
}
