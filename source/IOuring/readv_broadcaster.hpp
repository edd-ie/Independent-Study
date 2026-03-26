//
// Created by _edd.ie_ on 16/02/2026.
//

#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <vector>
#include <chrono>
#include <print>
#include <unistd.h>
#include <omp.h>
#include <cmath>
#include <algorithm>

#include "../file_system/IO_Handle.hpp"

const size_t CHUNK_SIZE = 64 * 1024;

ssize_t submit_read_write(IO_Handle &input, IO_Handle &output, off_t start_offset)
{
    const off_t file_sz = IO_Handle::get_file_size(input.native_handle());
    if (file_sz <= 0)
        return 0;

    // Ensure we don't exceed IOV_MAX (usually 1024)
    const size_t MAX_BLOCKS = 1024;
    const size_t WINDOW_SIZE = MAX_BLOCKS * CHUNK_SIZE; // Effectively 64MB if chunk is 64KB

    off_t total_processed = 0;

    while (total_processed < file_sz)
    {
        size_t remaining = file_sz - total_processed;
        size_t current_window = std::min<size_t>(WINDOW_SIZE, remaining);
        int blocks = (current_window + CHUNK_SIZE - 1) / CHUNK_SIZE;

        std::vector<iovec> iovecs(blocks);

        // Allocation phase
        for (int i = 0; i < blocks; ++i)
        {
            size_t bytes_to_read = std::min<size_t>(CHUNK_SIZE, current_window - (i * CHUNK_SIZE));
            void *ptr = nullptr;
            if (posix_memalign(&ptr, 4096, CHUNK_SIZE) != 0)
                return -1;
            iovecs[i].iov_base = ptr;
            iovecs[i].iov_len = bytes_to_read;
        }

        // Use the absolute offset: start_offset (usually 0) + total_processed
        if (preadv(input.native_handle(), iovecs.data(), blocks, start_offset + total_processed) < 0)
        {
            perror("preadv failure");
            for (auto &v : iovecs)
                free(v.iov_base);
            return -1;
        }

        if (pwritev(output.native_handle(), iovecs.data(), blocks, start_offset + total_processed) < 0)
        {
            perror("pwritev failure");
            for (auto &v : iovecs)
                free(v.iov_base);
            return -1;
        }

        // Cleanup: Mandatory to prevent OOM on 512MB+ files
        for (auto &v : iovecs)
            free(v.iov_base);

        total_processed += current_window;
    }
    return total_processed;
}

ssize_t perform_readv_broadcast(IO_Handle &root_input, std::span<IO_Handle> output_files)
{
    int num_outputs = static_cast<int>(output_files.size());
    int total_nodes = num_outputs + 1;
    int total_rounds = std::ceil(std::log2(total_nodes));

    size_t data_transfer = 0;

    for (int round = 0; round < total_rounds; ++round)
    {
        int stride = 1 << round;

#pragma omp parallel for num_threads(4) shared(root_input, output_files) firstprivate(stride) reduction(+ : data_transfer)
        for (int i = 0; i < stride; ++i)
        {
            int sender_id = i;
            int receiver_id = i + stride;

            if (receiver_id < total_nodes)
            {
                int dest_idx = receiver_id - 1;

                if (sender_id == 0)
                {
                    data_transfer += submit_read_write(root_input, output_files[dest_idx], 0);
                }
                else
                {
                    int src_idx = sender_id - 1;
                    data_transfer += submit_read_write(output_files[src_idx], output_files[dest_idx], 0);
                }
            }
        }
    }

    std::println("Total : {}, file : {}", data_transfer, (IO_Handle::get_file_size(root_input.native_handle()) * output_files.size()));
    return data_transfer;
}
