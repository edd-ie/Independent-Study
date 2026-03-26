// Created by _edd.ie_ on 16/02/2026.
//

#pragma once

#include <liburing.h>
#include <vector>
#include <algorithm>
#include <span>
#include <iostream>
#include "../file_system/IO_Handle.hpp"

const size_t BLOCK_SZ = 64 * 1024; // 64KB to stay under IOV_MAX (1024)
const size_t WINDOW_SIZE = 8 * 1024 * 1024;

struct RequestContext
{
    std::vector<iovec> iovecs;
    std::vector<void *> raw_pointers; // Track for freeing
    size_t bytes;

    RequestContext(int blocks, size_t bytes_to_process) : bytes(bytes_to_process)
    {
        iovecs.resize(blocks);
        size_t remaining = bytes_to_process;

        for (int i = 0; i < blocks; ++i)
        {
            size_t to_read = std::min<size_t>(remaining, BLOCK_SZ);
            void *ptr = nullptr;
            // Align to 4096 for O_DIRECT and high-perf filesystem compatibility
            if (posix_memalign(&ptr, 4096, BLOCK_SZ) != 0)
                throw std::bad_alloc();

            raw_pointers.push_back(ptr);
            iovecs[i].iov_base = ptr;
            iovecs[i].iov_len = to_read;
            remaining -= to_read;
        }
    }

    ~RequestContext()
    {
        for (void *p : raw_pointers)
            free(p);
    }
};

size_t submit_readV_writeV(io_uring &ring, IO_Handle &input, IO_Handle &output, off_t start_offset)
{
    const size_t file_sz = static_cast<size_t>(IO_Handle::get_file_size(input.native_handle()));
    if (file_sz <= 0)
        return 0;

    size_t total_processed = 0;

    while (total_processed < file_sz)
    {
        size_t bytes_to_process = std::min<size_t>(WINDOW_SIZE, file_sz - total_processed);
        uint blocks = (bytes_to_process + BLOCK_SZ - 1) / BLOCK_SZ;

        auto *ctx = new RequestContext(blocks, bytes_to_process);

        // 1. Readv SQE
        io_uring_sqe *sqe = io_uring_get_sqe(&ring);
        if (!sqe)
        {
            io_uring_submit(&ring);
            sqe = io_uring_get_sqe(&ring);
        }
        io_uring_prep_readv(sqe, input.native_handle(), ctx->iovecs.data(), blocks, start_offset + total_processed);
        sqe->flags |= IOSQE_IO_LINK; // Link read to write
        io_uring_sqe_set_data(sqe, nullptr);

        // 2. Writev SQE
        sqe = io_uring_get_sqe(&ring);
        if (!sqe)
        {
            io_uring_submit(&ring);
            sqe = io_uring_get_sqe(&ring);
        }
        io_uring_prep_writev(sqe, output.native_handle(), ctx->iovecs.data(), blocks, start_offset + total_processed);
        io_uring_sqe_set_data(sqe, ctx); // Only the last link carries the context

        total_processed += bytes_to_process;
    }

    io_uring_submit(&ring);
    return total_processed;
}

size_t perform_uring_broadcast(IO_Handle &root_input, std::span<IO_Handle> output_files)
{
    io_uring ring{};
    // Increased queue size to handle fan-out depth
    if (io_uring_queue_init(1024, &ring, 0) < 0)
        return 0;

    size_t sz = IO_Handle::get_file_size(root_input.native_handle());
    // Calculate exact number of windows
    size_t windows_per_file = (sz + WINDOW_SIZE - 1) / WINDOW_SIZE;
    size_t total_expected_completions = windows_per_file * output_files.size();

    for (auto &dest : output_files)
    {
        submit_readV_writeV(ring, root_input, dest, 0);
    }

    size_t completed = 0;
    size_t total_data_out = 0;

    while (completed < total_expected_completions)
    {
        io_uring_cqe *cqe;
        if (io_uring_wait_cqe(&ring, &cqe) < 0)
            break;

        if (io_uring_cqe_get_data(cqe))
        {
            RequestContext *ctx = static_cast<RequestContext *>(io_uring_cqe_get_data(cqe));
            if (cqe->res < 0)
            {
                std::println(stderr, "Uring Error: {}", strerror(-cqe->res));
            }
            else
            {
                total_data_out += ctx->bytes;
            }
            delete ctx;
            completed++;
        }
        io_uring_cqe_seen(&ring, cqe);
    }

    io_uring_queue_exit(&ring);
    return total_data_out;
}
