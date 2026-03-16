#pragma once

#include <sys/stat.h>
#include <print>
#include <liburing.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/uio.h>
#include "Request.hpp"

namespace Network
{
    const int SPLICE_Q_DEPTH = 64;
    const int SPLICE_CHUNK = 4096;

    void submit_splice(io_uring *ring, std::shared_ptr<IO_Handle> inputFD, std::unique_ptr<Network::Request> request)
    {
        io_uring_sqe *sqe = io_uring_get_sqe(ring);
        if (!sqe)
        {
            std::println(stderr, "Error getting sqe");
            exit(1);
        }

        Network::Request *raw_ptr = request.release();
        io_uring_prep_splice(sqe, inputFD->native_handle(), -1, raw_ptr->getFile(), -1, raw_ptr->bytes(), SPLICE_F_MOVE);
        io_uring_sqe_set_data(sqe, raw_ptr);
    }

    void broadcastSplice(std::shared_ptr<IO_Handle> inputFD, std::vector<std::shared_ptr<IO_Handle>> &outputFDs)
    {
        io_uring ring{};
        if (io_uring_queue_init(SPLICE_Q_DEPTH, &ring, 0) < 0)
        {
            std::println(stderr, "Failure to init queue!");
            exit(1);
        }

        int splice_pending = 0;

        auto start_splice = [&]()
        {
            splice_pending = outputFDs.size();
            auto buffer = std::make_shared<std::vector<uint8_t>>(SPLICE_CHUNK);
            for (std::shared_ptr<IO_Handle> outFD : outputFDs)
            {

                auto req = std::make_unique<Network::Request>(
                    Network::OpType::Splice,
                    buffer,
                    outFD,
                    SPLICE_CHUNK);

                submit_splice(&ring, inputFD, std::move(req));
                if (io_uring_submit(&ring) < 0)
                {
                    std::println(stderr, "submit for read failed");
                }
            }
        };

        start_splice();

        while (true)
        {
            io_uring_cqe *cqe;

            if (io_uring_wait_cqe(&ring, &cqe) < 0)
            {
                std::println(stderr, "wait_cqe failed");
                break;
            }

            std::unique_ptr<Network::Request> data(static_cast<Network::Request *>(io_uring_cqe_get_data(cqe)));
            int res = cqe->res;
            io_uring_cqe_seen(&ring, cqe);

            if (res < 0)
            {
                if (res == -EAGAIN) // pipe is currently empty.
                {
                    start_splice();
                    continue;
                }

                if (data->getType() == Network::OpType::Splice)
                {
                    std::println(stderr, "Removing dead client (FD: {}), Error: {}", data->getFile(), res);

                    outputFDs.erase(
                        std::remove_if(outputFDs.begin(), outputFDs.end(),
                                       [&](const std::shared_ptr<IO_Handle> &h)
                                       {
                                           return h->native_handle() == data->getFile();
                                       }),
                        outputFDs.end());

                    if (outputFDs.empty())
                    {
                        break;
                    }

                    splice_pending--;
                    if (splice_pending == 0 && !outputFDs.empty())
                    {
                        start_splice();
                    }
                    continue;
                }
                std::println(stderr, "CQE Error: {}", res);
                break;
            }

            if (res == 0)
                break;

            splice_pending--;
            if (splice_pending == 0)
            {
                start_splice();
            }
        }
        io_uring_queue_exit(&ring);
    }
}