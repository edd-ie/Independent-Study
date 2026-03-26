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

    const unsigned int CHUNK_SIZE = 512 * 1024;

    uintptr_t pack_data(IO_Handle::native_handle_type fd, off_t offset, size_t size)
    {
        uintptr_t packed = 0;

        packed |= (static_cast<uintptr_t>(fd) & 0xF) << 60;        // 4 bits for flags
        packed |= (static_cast<uintptr_t>(size) & 0xFFFFF) << 40;  // 20 bits for size (up to 1MB)
        packed |= (static_cast<uintptr_t>(offset) & 0xFFFFFFFFFF); // 40 bits for offset (up to 1TB)

        return packed;
    }

    // Returns 1 if successful
    size_t prepare_tee(io_uring &ring, IO_Handle &in, IO_Handle &out, uint size, bool link = false)
    {
        io_uring_sqe *sqe = io_uring_get_sqe(&ring);
        if (!sqe)
            return 0;

        io_uring_prep_tee(sqe, in.native_handle(), out.native_handle(), size, 0);

        if (link)
            sqe->flags |= IOSQE_IO_LINK;

        uintptr_t packed = pack_data(out.native_handle(), 0, size);
        io_uring_sqe_set_data(sqe, reinterpret_cast<void *>(packed));
        return 1;
    }

    size_t prepare_splice(io_uring &ring, IO_Handle &in, IO_Handle &out, OP_FLAGS flag, int64_t offset, size_t size, bool link = false)
    {
        io_uring_sqe *sqe = io_uring_get_sqe(&ring);
        if (!sqe)
            return 0;

        int64_t off_in = -1, off_out = -1;

        switch (flag)
        {
        case OP_FLAGS::SRC_SPLICE_PIPE:
            off_in = offset;
            break;
        case OP_FLAGS::PIPE_SPLICE_DEST:
            off_out = offset;
            break;
        default:
            break;
        }

        io_uring_prep_splice(sqe, in.native_handle(), off_in, out.native_handle(), off_out, size, SPLICE_F_MOVE);

        if (link)
            sqe->flags |= IOSQE_IO_LINK;

        uintptr_t packed = pack_data(out.native_handle(), offset, size);
        io_uring_sqe_set_data(sqe, reinterpret_cast<void *>(packed));
        return 1;
    }

    size_t process_cqes(io_uring &ring)
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
            IO_Handle::native_handle_type fd = static_cast<IO_Handle::native_handle_type>((packed >> 60) & 0xF);
            size_t size = static_cast<size_t>((packed >> 40) & 0xFFFFF);
            off_t offset = static_cast<off_t>(packed & 0xFFFFFFFFFF);

            if (cqe->res < 0)
            {
                std::println(stderr, "Splice Error: {} on file: {} data size: {} offset: {}", strerror(-cqe->res), fd, size, offset);
                break;
            }

            completed_cqes++;
        }
        io_uring_cq_advance(&ring, completed_cqes);
        return total_data;
    }

    size_t perform_tee_broadcast(IO_Handle &source_file, IO_Handle &src_write_pipe, IO_Handle &src_read_pipe, std::span<IO_Handle> &output_files, std::span<IO_Handle> dest_write_pipes, std::span<IO_Handle> dest_read_pipes)
    {
        io_uring ring{};
        if (io_uring_queue_init(1024, &ring, 0) < 0)
        {
            std::println(stderr, "Failure to init queue!");
            exit(EIO);
        }

        size_t file_sz = static_cast<size_t>(IO_Handle::get_file_size(source_file.native_handle()));

        size_t to_process = 0, total_processed = 0, total_data_moved = 0;
        uint submissions;

        const int BATCH_SIZE = 8; // Number of chunks to keep in the pipeline
        int in_flight = 0;

        while (total_processed < file_sz || in_flight > 0)
        {

            while (in_flight < BATCH_SIZE && total_processed < file_sz)
            {
                size_t to_process = std::min<size_t>(CHUNK_SIZE, file_sz - total_processed);

                io_uring_sqe *sqe = io_uring_get_sqe(&ring);
                io_uring_prep_splice(sqe, source_file.native_handle(), total_processed,
                                     src_write_pipe.native_handle(), -1, to_process, SPLICE_F_MORE);
                sqe->flags |= IOSQE_IO_LINK; // Link only to the START of the broadcast
                sqe->user_data = 0;

                // BROADCAST BRANCHES (Parallel)
                for (size_t i = 0; i < output_files.size() - 1; i++)
                {

                    sqe = io_uring_get_sqe(&ring);
                    io_uring_prep_tee(sqe, src_read_pipe.native_handle(), dest_write_pipes[i].native_handle(), to_process, 0);
                    sqe->flags |= IOSQE_IO_LINK;
                    sqe->user_data = 0;

                    sqe = io_uring_get_sqe(&ring);
                    io_uring_prep_splice(sqe, dest_read_pipes[i].native_handle(), -1, output_files[i].native_handle(), total_processed, to_process, SPLICE_F_MORE);
                    sqe->user_data = 0;
                }

                // FINAL DRAIN (The anchor for this chunk)
                sqe = io_uring_get_sqe(&ring);
                io_uring_prep_splice(sqe, src_read_pipe.native_handle(), -1, output_files.back().native_handle(), total_processed, to_process, SPLICE_F_MORE);
                // Mark this as the chunk tracker
                io_uring_sqe_set_data(sqe, reinterpret_cast<void *>(static_cast<uintptr_t>(to_process)));

                total_processed += to_process;
                in_flight++;
            }

            // 2. Submit and Reap
            io_uring_submit_and_wait(&ring, 1);

            io_uring_cqe *cqe;
            unsigned head;
            int count = 0;
            io_uring_for_each_cqe(&ring, head, cqe)
            {
                uintptr_t bytes = reinterpret_cast<uintptr_t>(io_uring_cqe_get_data(cqe));
                if (bytes > 0)
                {
                    if (cqe->res >= 0)
                        total_data_moved += (bytes * output_files.size());
                    else if (cqe->res < 0)
                    {
                        std::println(stderr, "Write failed: {}", strerror(-cqe->res));
                    }
                    in_flight--;
                }
                count++;
            }
            io_uring_cq_advance(&ring, count);
        }

        // while (total_processed < file_sz)
        // {
        //     to_process = std::min<size_t>(static_cast<size_t>(CHUNK_SIZE), file_sz - total_processed);
        //     submissions = 0;

        //     // 1. DISK -> SOURCE PIPE (Must use file offset)
        //     io_uring_sqe *sqe = io_uring_get_sqe(&ring);
        //     io_uring_prep_splice(sqe, source_file.native_handle(), total_processed,
        //                          src_write_pipe.native_handle(), -1,
        //                          to_process, SPLICE_F_MORE);
        //     sqe->flags |= IOSQE_IO_LINK;
        //     sqe->user_data = 0;
        //     submissions++;

        //     // 2. BROADCAST TO N-1 FILES
        //     for (size_t i = 0; i < output_files.size() - 1; i++)
        //     {
        //         // TEE: Source Pipe -> Dest Pipe (No offsets)
        //         sqe = io_uring_get_sqe(&ring);
        //         io_uring_prep_tee(sqe, src_read_pipe.native_handle(),
        //                           dest_write_pipes[i].native_handle(), to_process, 0);
        //         sqe->flags |= IOSQE_IO_LINK;
        //         sqe->user_data = 0;
        //         submissions++;

        //         // SPLICE: Dest Pipe -> File (Must use file offset)
        //         sqe = io_uring_get_sqe(&ring);
        //         io_uring_prep_splice(sqe, dest_read_pipes[i].native_handle(), -1,
        //                              output_files[i].native_handle(), total_processed,
        //                              to_process, SPLICE_F_MORE);
        //         sqe->flags |= IOSQE_IO_LINK;
        //         sqe->user_data = to_process;
        //         submissions++;
        //     }

        //     // 3. FINAL SPLICE: SOURCE PIPE -> LAST FILE (Consumes source pipe, uses file offset)
        //     sqe = io_uring_get_sqe(&ring);
        //     io_uring_prep_splice(sqe, src_read_pipe.native_handle(), -1,
        //                          output_files.back().native_handle(), total_processed,
        //                          to_process, SPLICE_F_MORE);
        //     // Mark for accounting
        //     io_uring_sqe_set_data(sqe, reinterpret_cast<void *>(static_cast<uintptr_t>(to_process)));
        //     submissions++;

        //     total_processed += to_process;

        //     io_uring_submit_and_wait(&ring, submissions);
        //     io_uring_cqe *cqe;
        //     unsigned head;
        //     int count = 0;
        //     io_uring_for_each_cqe(&ring, head, cqe)
        //     {
        //         uintptr_t data = reinterpret_cast<uintptr_t>(io_uring_cqe_get_data(cqe));
        //         if (data > 0)
        //         {
        //             if (cqe->res >= 0)
        //                 total_data_moved += data;
        //             else
        //                 std::println(stderr, "Chain Failure: {}", strerror(-cqe->res));
        //         }
        //         count++;
        //     }
        //     io_uring_cq_advance(&ring, count);
        // }

        io_uring_queue_exit(&ring);
        // return total_processed * output_files.size();
        return total_data_moved;
    }

}