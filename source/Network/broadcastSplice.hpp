#pragma once

#include <sys/stat.h>
#include <print>
#include <liburing.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/uio.h>
#include "Request.hpp"
#include "../file_system/IO_Handle.hpp"
#include <cstring>

const size_t SPLICE_CHUNK_SIZE = 512 * 1024;

size_t prepare_splice(io_uring &ring, IO_Handle &input, IO_Handle &output, IO_Handle &pipe_r, IO_Handle &pipe_w)
{
    const off_t file_sz = IO_Handle::get_file_size(input.native_handle());
    off_t total_processed = 0;
    size_t sqe_count = 0;
    io_uring_sqe *sqe;
    uintptr_t packed;

    while (total_processed < file_sz)
    {
        size_t to_splice = std::min<size_t>(SPLICE_CHUNK_SIZE, file_sz - total_processed);

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

        // // sqe->flags |= IOSQE_IO_LINK;

        // std::println("Read before write loop proc: {} file: {} to_splice: {}", total_processed, file_sz, to_splice);

        total_processed += to_splice;
        sqe_count += 2;
    }
    return sqe_count;
}

size_t perform_splice_broadcast(IO_Handle &source_file, std::span<IO_Handle> output_files,
                                std::span<IO_Handle> dest_write_pipes, std::span<IO_Handle> dest_read_pipes)
{
    io_uring ring{};
    if (io_uring_queue_init(512, &ring, 0) < 0)
    {
        std::println(stderr, "Failure to init queue!");
        return 0;
    }

    size_t expected_cqes = 0;
    for (size_t i = 0; i < output_files.size(); i++)
    {
        expected_cqes += prepare_splice(ring, source_file, output_files[i],
                                        dest_read_pipes[i], dest_write_pipes[i]);
    }

    io_uring_submit(&ring);

    int ret = io_uring_submit_and_wait(&ring, expected_cqes);
    if (ret < 0 || ret == -errno)
        return 0;

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
            std::println(stderr, "Splice Error: {}", strerror(-cqe->res));
        }

        uintptr_t packed = reinterpret_cast<uintptr_t>(io_uring_cqe_get_data(cqe));
        total_data += packed & 0xFFFFFFFF;

        completed_cqes++;
    }
    io_uring_cq_advance(&ring, completed_cqes);

    io_uring_queue_exit(&ring);
    return total_data;
}
