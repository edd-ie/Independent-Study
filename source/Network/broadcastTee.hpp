#pragma once

#include <sys/stat.h>
#include <print>
#include <liburing.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/uio.h>
#include <memory>
#include "Request.hpp"

namespace Network
{
    size_t submit_batch(io_uring *ring, std::shared_ptr<Util::IO_Handle> inputFD,
                        std::vector<std::shared_ptr<Util::IO_Handle>> &outputFDs, size_t len)
    {
        if (outputFDs.empty())
            return 0;

        for (size_t i = 0; i < outputFDs.size(); ++i)
        {
            io_uring_sqe *sqe = io_uring_get_sqe(ring);
            if (!sqe)
                break;

            bool is_last = (i == outputFDs.size() - 1);

            auto type = is_last ? Network::OpType::Splice : Network::OpType::Tee;
            auto req = std::make_unique<Network::Request>(
                type,
                std::make_shared<std::vector<uint8_t>>(), // Empty vector as placeholder
                outputFDs[i],
                len);

            if (!is_last)
            {
                io_uring_prep_tee(sqe, inputFD->native_handle(), req->getFile(), len, SPLICE_F_NONBLOCK);
                sqe->flags |= IOSQE_IO_LINK;
            }
            else
            {
                io_uring_prep_splice(sqe, inputFD->native_handle(), -1, req->getFile(), -1, len, SPLICE_F_MOVE | SPLICE_F_NONBLOCK);
            }

            io_uring_sqe_set_data(sqe, req.release());
        }

        io_uring_submit(ring);
        return outputFDs.size();
    }

    void broadcastTee(std::shared_ptr<Util::IO_Handle> inputFD, std::vector<std::shared_ptr<Util::IO_Handle>> &outputFDs)
    {
        io_uring ring{};
        if (io_uring_queue_init(64, &ring, 0) < 0)
        {
            std::println(stderr, "Failure to init queue!");
            exit(1);
        }

        size_t pending_completions = submit_batch(&ring, inputFD, outputFDs, 4096);

        while (!outputFDs.empty())
        {
            io_uring_cqe *cqe;
            if (io_uring_wait_cqe(&ring, &cqe) < 0)
                break;

            std::unique_ptr<Network::Request> req(static_cast<Network::Request *>(io_uring_cqe_get_data(cqe)));
            int res = cqe->res;
            io_uring_cqe_seen(&ring, cqe);
            pending_completions--;

            if (res < 0)
            {
                if (res == -EAGAIN)
                {
                    // If pipe is empty/full,retry this batch later
                                }
                else
                {
                    std::println(stderr, "Error on FD {}: {}", req->getFile(), res);
                    // Remove dead pipe
                    outputFDs.erase(std::remove_if(outputFDs.begin(), outputFDs.end(),
                                                   [&](const auto &h)
                                                   { return h->native_handle() == req->getFile(); }),
                                    outputFDs.end());
                }
            }
            else if (res == 0)
            {
                // EOF on input or pipe closed
                std::println("Stream ended.");
                goto cleanup;
            }

            // current batch is fully finished
            if (pending_completions == 0)
            {
                pending_completions = submit_batch(&ring, inputFD, outputFDs, 4096);
            }
        }

    cleanup:
        io_uring_queue_exit(&ring);
    }
}