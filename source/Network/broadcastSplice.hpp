#pragma once

#include <liburing.h>
#include <algorithm>
#include <span>
#include <iostream>
#include "../file_system/IO_Handle.hpp"

const size_t SPLICE_CHUNK_SIZE = 512 * 1024; // 512KB

size_t prepare_splice(io_uring &ring, IO_Handle &input, IO_Handle &output, IO_Handle &pipe_r, IO_Handle &pipe_w)
{
    const off_t file_sz = IO_Handle::get_file_size(input.native_handle());
    off_t total_processed = 0;
    size_t sqe_count = 0;
    io_uring_sqe *sqe;
    uintptr_t packed;

    while (total_processed < file_sz)
    {
        size_t to_splice = std::min<size_t>(SPLICE_CHUNK_SIZE, static_cast<size_t>(file_sz - total_processed));

        sqe = io_uring_get_sqe(&ring);

        io_uring_prep_splice(sqe, pipe_r.native_handle(), -1,
                             output.native_handle(), total_processed,
                             to_splice, SPLICE_F_MOVE);

        packed = (static_cast<uintptr_t>(output.native_handle()) << 32) | (to_splice & 0xFFFFFFFF);
        io_uring_sqe_set_data(sqe, reinterpret_cast<void *>(packed));

        sqe = io_uring_get_sqe(&ring);

        io_uring_prep_splice(sqe, input.native_handle(), total_processed,
                             pipe_w.native_handle(), -1,
                             to_splice, SPLICE_F_MOVE);

        packed = (static_cast<uintptr_t>(output.native_handle()) << 32) | (0 & 0xFFFFFFFF);
        io_uring_sqe_set_data(sqe, reinterpret_cast<void *>(packed));

        total_processed += to_splice;
        sqe_count += 2;
    }
    return sqe_count;
}

size_t perform_splice_broadcast(IO_Handle &source_file, std::span<IO_Handle> output_files,
                                std::span<IO_Handle> dest_write_pipes, std::span<IO_Handle> dest_read_pipes)
{
    io_uring ring{};
    if (io_uring_queue_init(1024, &ring, 0) < 0)
        return 0;

    const off_t file_sz = IO_Handle::get_file_size(source_file.native_handle());
    off_t total_processed = 0;
    size_t total_data_verified = 0;
    size_t expected_cqes = 0;

    while (total_processed < file_sz)
    {
        size_t to_splice = std::min<size_t>(SPLICE_CHUNK_SIZE, static_cast<size_t>(file_sz - total_processed));

        for (size_t i = 0; i < output_files.size(); i++)
        {
            // 1. Splice from Source File to Pipe
            io_uring_sqe *sqe = io_uring_get_sqe(&ring);
            if (!sqe)
                break;

            io_uring_prep_splice(sqe, source_file.native_handle(), total_processed,
                                 dest_write_pipes[i].native_handle(), -1,
                                 to_splice, SPLICE_F_MOVE | SPLICE_F_MORE);
            sqe->flags |= IOSQE_IO_LINK;
            io_uring_sqe_set_data(sqe, nullptr);

            // Splice from Pipe to Destination File
            sqe = io_uring_get_sqe(&ring);
            if (!sqe)
                break;

            io_uring_prep_splice(sqe, dest_read_pipes[i].native_handle(), -1,
                                 output_files[i].native_handle(), total_processed,
                                 to_splice, SPLICE_F_MOVE);

            uintptr_t packed = (static_cast<uintptr_t>(to_splice) & 0xFFFFFFFF);
            io_uring_sqe_set_data(sqe, reinterpret_cast<void *>(packed));

            expected_cqes += 2;
        }

        total_processed += to_splice;

        // PERIODIC SUBMISSION & REAPING
        // Prevents SQE/CQE overflow for 512MB+ files
        if (expected_cqes >= 512 || total_processed >= file_sz)
        {
            io_uring_submit(&ring);

            while (expected_cqes > 0)
            {
                io_uring_cqe *cqe;
                int ret = io_uring_wait_cqe_nr(&ring, &cqe, 1);
                if (ret < 0)
                    break;

                if (io_uring_cqe_get_data(cqe))
                {
                    if (cqe->res > 0)
                        total_data_verified += cqe->res;
                }

                io_uring_cqe_seen(&ring, cqe);
                expected_cqes--;
            }
        }
    }

    io_uring_queue_exit(&ring);
    return total_data_verified;
}
