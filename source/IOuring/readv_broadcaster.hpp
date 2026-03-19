//
// Created by _edd.ie_ on 16/02/2026.
//

#pragma once

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
#include <omp.h>
#include <cmath>
#include <algorithm>

#include "../file_system/IO_Handle.hpp"

const size_t CHUNK_SIZE = 4096;

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

ssize_t submit_read_write(IO_Handle &input, IO_Handle &output, off_t start_offset)
{
    const off_t file_sz = get_file_size(input.native_handle());
    if (file_sz <= 0)
        return file_sz;

    const size_t WINDOW_SIZE = 8 * 1024 * 1024;
    off_t total_processed = 0;

    while (total_processed < file_sz)
    {
        off_t remaining_in_file = file_sz - total_processed;
        size_t current_window_bytes = std::min<size_t>(WINDOW_SIZE, remaining_in_file);

        int blocks = (current_window_bytes + CHUNK_SIZE - 1) / CHUNK_SIZE;

        std::vector<iovec> iovecs(blocks);
        std::vector<std::vector<char>> buffer_pool;
        buffer_pool.reserve(blocks);

        size_t window_remaining = current_window_bytes;
        for (int i = 0; i < blocks; ++i)
        {
            size_t bytes_to_read = std::min<size_t>(window_remaining, CHUNK_SIZE);

            buffer_pool.emplace_back(CHUNK_SIZE);
            iovecs[i].iov_base = buffer_pool.back().data();
            iovecs[i].iov_len = bytes_to_read;

            window_remaining -= bytes_to_read;
        }

        if (preadv(input.native_handle(), iovecs.data(), blocks, start_offset) < 0)
        {
            perror("preadv failure");
            return -1;
        }

        if (pwritev(output.native_handle(), iovecs.data(), blocks, start_offset) < 0)
        {
            perror("pwritev failure");
            return -1;
        }

        total_processed += current_window_bytes;
    }

    return static_cast<ssize_t>(total_processed);
}

int perform_tree_broadcast(IO_Handle &root_input, std::span<IO_Handle> output_files)
{
    int num_outputs = static_cast<int>(output_files.size());
    int total_nodes = num_outputs + 1;
    int total_rounds = std::ceil(std::log2(total_nodes));

    for (int round = 0; round < total_rounds; ++round)
    {
        int stride = 1 << round;

#pragma omp parallel for num_threads(4)
        for (int i = 0; i < stride; ++i)
        {
            int sender_id = i;
            int receiver_id = i + stride;

            if (receiver_id < total_nodes)
            {
                int dest_idx = receiver_id - 1;

                if (sender_id == 0)
                {
                    submit_read_write(root_input, output_files[dest_idx], 0);
                }
                else
                {
                    int src_idx = sender_id - 1;
                    submit_read_write(output_files[src_idx], output_files[dest_idx], 0);
                }
            }
        }
    }
    return 0;
}
