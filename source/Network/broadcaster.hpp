#ifndef BROADCASTER_H
#define BROADCASTER_H

#include <sys/types.h>
#include <sys/stat.h>
#include <print>
#include <liburing.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/uio.h>
#include <vector>
#include "Request.hpp"

namespace broadcaster
{
    const int Q_DEPTH = 64;
    const int CHUNK = 4096;

    void submit_read(io_uring *ring, std::unique_ptr<Network::Request> read_req)
    {
        io_uring_sqe *sqe = io_uring_get_sqe(ring);
        if (!sqe)
        {
            std::println(stderr, "Error getting sqe");
            exit(1);
        }

        Network::Request *raw_ptr = read_req.release();

        io_uring_prep_read(sqe,
                           raw_ptr->getFile(),
                           raw_ptr->getData(),
                           CHUNK, 0);

        io_uring_sqe_set_data(sqe, raw_ptr);
        io_uring_submit(ring);
    }

    void submit_write(io_uring *ring, std::unique_ptr<Network::Request> write_pipe)
    {
        io_uring_sqe *sqe = io_uring_get_sqe(ring);
        if (!sqe)
        {
            std::println(stderr, "Error getting sqe");
            exit(1);
        }

        Network::Request *raw_ptr = write_pipe.release();

        io_uring_prep_write(sqe, raw_ptr->getFile(),
                            raw_ptr->getData(), raw_ptr->bytes(), 0);
        io_uring_sqe_set_data(sqe, raw_ptr);
    }

    void broadcast(std::shared_ptr<Util::IO_Handle> inputFD, std::vector<std::shared_ptr<Util::IO_Handle>> &outputFDs)
    {
        io_uring ring{};
        if (io_uring_queue_init(Q_DEPTH, &ring, 0) < 0)
        {
            std::println(stderr, "Failure to init queue!");
            exit(1);
        }

        int writes_pending = 0;

        auto start_read = [&]()
        {
            auto read_req = std::make_unique<Network::Request>(
                Network::OpType::Read,
                std::make_shared<std::vector<uint8_t>>(CHUNK),
                inputFD, 0);
            submit_read(&ring, std::move(read_req));
        };

        start_read();

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
                    submit_read(&ring, std::move(data));
                    continue;
                }

                if (data->getType() == Network::OpType::Write)
                {
                    std::println(stderr, "Removing dead client (FD: {}), Error: {}", data->getFile(), res);

                    outputFDs.erase(
                        std::remove_if(outputFDs.begin(), outputFDs.end(),
                                       [&](const std::shared_ptr<Util::IO_Handle> &h)
                                       {
                                           return h->native_handle() == data->getFile();
                                       }),
                        outputFDs.end());

                    writes_pending--;
                    if (writes_pending == 0 && !outputFDs.empty())
                    {
                        start_read();
                    }
                    continue;
                }
                std::println(stderr, "CQE Error: {}", res);
                break;
            }

            if (data->getType() == Network::OpType::Read)
            {
                if (res == 0)
                    break; // EOF

                writes_pending = outputFDs.size();
                auto share_data = data->getSharedBuffer();
                for (std::shared_ptr<Util::IO_Handle> outFD : outputFDs)
                {

                    auto write_req = std::make_unique<Network::Request>(
                        Network::OpType::Write,
                        share_data,
                        outFD,
                        static_cast<size_t>(res));

                    submit_write(&ring, std::move(write_req));
                }

                if (io_uring_submit(&ring) < 0)
                {
                    std::println(stderr, "submit for read failed");
                }
            }
            else if (data->getType() == Network::OpType::Write)
            {
                writes_pending--;
                if (writes_pending == 0)
                {
                    start_read();
                }
            }
        }
        io_uring_queue_exit(&ring);
    }
}
#endif // BROADCASTER_H