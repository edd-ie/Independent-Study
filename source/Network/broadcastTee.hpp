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

    size_t prepare_tee(io_uring &ring, IO_Handle &input_pipe_r, IO_Handle &output_pipe_w, int i)
    {
        io_uring_sqe *sqe = io_uring_get_sqe(&ring);
        if (!sqe)
            return 0;

        io_uring_prep_tee(sqe,
                          input_pipe_r.native_handle(),
                          output_pipe_w.native_handle(),
                          CHUNK_SIZE,
                          SPLICE_F_NONBLOCK);

        uintptr_t packed = (static_cast<uintptr_t>(OP_FLAGS::PIPE_TEE_PIPE) << 32) | (i & 0xFFFFFFFF);
        io_uring_sqe_set_data(sqe, reinterpret_cast<void *>(packed));
        return 1;
    }

    size_t prepare_splice(io_uring &ring, IO_Handle &input_fd, IO_Handle &output_fd, const OP_FLAGS flag, const int offset, const int i)
    {

        io_uring_sqe *sqe = io_uring_get_sqe(&ring);
        if (!sqe)
            return 0;

        int64_t off_in = -1;
        int64_t off_out = -1;

        if (flag == OP_FLAGS::SRC_SPLICE_PIPE)
        {
            off_in = offset;
        }
        else if (flag == OP_FLAGS::PIPE_SPLICE_DEST)
        {
            off_out = offset;
        }

        io_uring_prep_splice(sqe,
                             input_fd.native_handle(), off_in,
                             output_fd.native_handle(), off_out,
                             CHUNK_SIZE,
                             SPLICE_F_MOVE | SPLICE_F_NONBLOCK);

        uintptr_t packed = (static_cast<uintptr_t>(flag) << 32) | (i & 0xFFFFFFFF);
        io_uring_sqe_set_data(sqe, reinterpret_cast<void *>(packed));
        return 1;
    }

    void broadcastTee(IO_Handle &source_file, IO_Handle &src_write_pipe, IO_Handle &src_read_pipe, std::span<IO_Handle> &output_files, std::span<IO_Handle> dest_write_pipes, std::span<IO_Handle> dest_read_pipes)
    {
        io_uring ring{};
        if (io_uring_queue_init(128, &ring, 0) < 0)
        {
            std::println(stderr, "Failure to init queue!");
            exit(EIO);
        }

        uint64_t src_offset = 0;
        std::vector<uint64_t> dest_offsets(output_files.size(), 0);
        int pending_completions = 0;

        auto submit_batch = [&]()
        {
            prepare_splice(ring, source_file, src_write_pipe, OP_FLAGS::SRC_SPLICE_PIPE, src_offset, 999);
            io_uring_get_sqe(&ring)->flags |= IOSQE_IO_LINK;
            pending_completions++;

            // 2. BRANCHING: Main Pipe -> Branch Pipes
            for (size_t i = 0; i < dest_write_pipes.size(); ++i)
            {
                bool is_last = (i == dest_write_pipes.size() - 1);

                if (!is_last)
                    prepare_tee(ring, src_read_pipe, dest_write_pipes[i], i);
                else
                    prepare_splice(ring, src_read_pipe, dest_write_pipes[i], OP_FLAGS::PIPE_SPLICE_PIPE, -1, i);

                io_uring_get_sqe(&ring)->flags |= IOSQE_IO_LINK;

                prepare_splice(ring, dest_read_pipes[i], output_files[i],
                               OP_FLAGS::PIPE_SPLICE_DEST, dest_offsets[i], i);

                pending_completions += 2;
            }

            src_offset += CHUNK_SIZE;
            io_uring_submit(&ring);
        };

        submit_batch();
        submit_batch();

        while (true)
        {
            int ret = io_uring_submit_and_wait(&ring, pending_completions);
            if (ret < 0)
                break;

            io_uring_cqe *cqe;
            if (io_uring_wait_cqe(&ring, &cqe) < 0)
                break;

            unsigned head;
            int processed = 0;
            bool stop_requested = false;

            io_uring_for_each_cqe(&ring, head, cqe)
            {
                processed++;
                uintptr_t packed = reinterpret_cast<uintptr_t>(io_uring_cqe_get_data(cqe));
                OP_FLAGS type = static_cast<OP_FLAGS>(packed >> 32);
                size_t index = packed & 0xFFFFFFFF;
                int res = cqe->res;

                if (res == -EAGAIN || res == -ECANCELED)
                    continue;

                if (res < 0)
                {
                    std::string err_src;
                    switch (type)
                    {
                    case OP_FLAGS::SRC_SPLICE_PIPE:
                        err_src = "Source File";
                        break;
                    case OP_FLAGS::PIPE_SPLICE_DEST:
                        err_src = output_files[index].get_name();
                        break;
                    default:
                        err_src = "Internal Pipe";
                        break;
                    }

                    std::println(stderr, "Error {}: Dead client/resource {}", res, err_src);
                    stop_requested = true;
                    break;
                }
                else if (type == OP_FLAGS::SRC_SPLICE_PIPE)
                {
                    if (res == 0)
                    {
                        std::println("Broadcast complete shutting down...");
                        stop_requested = true;
                        break;
                    }

                    submit_batch();
                }
                else if (type == OP_FLAGS::PIPE_SPLICE_DEST)
                {
                    if (res > 0)
                        dest_offsets[index] += res;
                }
            }
            io_uring_cq_advance(&ring, processed);
            if (stop_requested)
                break;
        }
        io_uring_queue_exit(&ring);
    }
}
