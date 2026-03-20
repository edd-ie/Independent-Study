// Created by _edd.ie_ on 16/02/2026.
//

#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <liburing.h>
#include <cstdint>
#include <memory>
#include <vector>
#include <print>
#include <algorithm>
#include <span>
#include "../file_system/IO_Handle.hpp"
#include "../Network/Request.hpp"

const int BLOCK_SZ = 4096;

struct RequestContext
{
    std::vector<iovec> iovecs;
    std::vector<std::vector<char>> buffer_pool;
    off_t offset;
    OpType type;
    size_t bytes;

    RequestContext(int blocks, size_t bytes_to_process)
    {
        iovecs.reserve(blocks);
        buffer_pool.reserve(blocks);
        bytes = bytes_to_process;
        size_t window_remaining = bytes_to_process;

        for (int i = 0; i < blocks; ++i)
        {
            size_t bytes_to_read = std::min<size_t>(window_remaining, BLOCK_SZ);

            buffer_pool.emplace_back(BLOCK_SZ);
            iovecs[i].iov_base = buffer_pool.back().data();
            iovecs[i].iov_len = bytes_to_read;

            window_remaining -= bytes_to_read;
        }
    }
};

ssize_t submit_req(io_uring &ring, IO_Handle &input, IO_Handle &output, off_t start_offset)
{
    const off_t file_sz = IO_Handle::get_file_size(input.native_handle());
    if (file_sz <= 0)
        return file_sz;

    const size_t WINDOW_SIZE = 8 * 1024 * 1024;
    off_t total_processed = 0;

    while (total_processed < file_sz)
    {
        off_t current_offset = start_offset + total_processed;
        size_t bytes_to_process = std::min<size_t>(WINDOW_SIZE, file_sz - total_processed);
        int blocks = (bytes_to_process + BLOCK_SZ - 1) / BLOCK_SZ;

        auto *ctx = new RequestContext(blocks, bytes_to_process);
        ctx->offset = current_offset;

        io_uring_sqe *sqe = io_uring_get_sqe(&ring);
        io_uring_prep_readv(sqe, input.native_handle(), ctx->iovecs.data(), blocks, current_offset);

        sqe->flags |= IOSQE_IO_LINK;

        io_uring_sqe_set_data(sqe, nullptr);

        sqe = io_uring_get_sqe(&ring);
        io_uring_prep_writev(sqe, output.native_handle(), ctx->iovecs.data(), blocks, current_offset);

        io_uring_sqe_set_data(sqe, ctx);

        total_processed += bytes_to_process;
    }

    io_uring_submit(&ring);
    return total_processed;
}

size_t perform_uring_broadcast(IO_Handle &root_input, std::span<IO_Handle> output_files)
{
    io_uring ring{};

    if (io_uring_queue_init(512, &ring, 0) < 0)
    {
        perror("io_uring_queue_init");
        return 0;
    }

    size_t total_expected_writes = 0;
    for (auto &dest : output_files)
    {
        submit_req(ring, root_input, dest, 0);

        off_t sz = IO_Handle::get_file_size(root_input.native_handle());
        total_expected_writes += (sz + (8 * 1024 * 1024) - 1) / (8 * 1024 * 1024);
    }

    size_t completed = 0;
    size_t read_data = 0;
    while (completed < total_expected_writes)
    {
        io_uring_cqe *cqe;
        int ret = io_uring_wait_cqe(&ring, &cqe);
        if (ret < 0)
            break;

        RequestContext *ctx = static_cast<RequestContext *>(io_uring_cqe_get_data(cqe));

        if (ctx)
        {
            read_data += ctx->bytes;

            delete ctx;
            completed++;
        }

        io_uring_cqe_seen(&ring, cqe);
    }

    io_uring_queue_exit(&ring);
    return read_data;
}
