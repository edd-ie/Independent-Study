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

namespace broadcaster
{

#define Q_DEPTH 64
#define CHUNK 4096

    struct Buffer
    {
        uint8_t data[CHUNK];
        int writes_remaining;
    };

    enum class OpType
    {
        Read,
        Write
    };

    struct Request
    {
        OpType type;
        Buffer *buffer;
        int fd; // only for write
        size_t bytes_to_write;
    };

    ssize_t FdGetFileSize(int fd)
    {
        struct stat st{};
        if (fstat(fd, &st) < 0)
            return -1;
        return st.st_size;
    }

    void submit_read(io_uring *ring, int inputFD, Request *read_pipe)
    {
        io_uring_sqe *sqe = io_uring_get_sqe(ring);
        if (!sqe)
        {
            std::println(stderr, "Error getting sqe");
            exit(1);
        }

        io_uring_prep_read(sqe, inputFD, read_pipe->buffer->data, CHUNK, 0);
        io_uring_sqe_set_data(sqe, read_pipe);

        if (io_uring_submit(ring) < 0)
        {
            std::println(stderr, "sumbit for read failed");
        }
    }

    void submit_write(io_uring *ring, int outputFD, Request *write_pipe)
    {
        io_uring_sqe *sqe = io_uring_get_sqe(ring);
        if (!sqe)
        {
            std::println(stderr, "Error getting sqe");
            exit(1);
        }

        io_uring_prep_write(sqe, outputFD, write_pipe->buffer->data, write_pipe->bytes_to_write, 0);
        io_uring_sqe_set_data(sqe, write_pipe);
    }

    void broadcast(const char *hostname, const std::vector<int> &outputFDs)
    {
        io_uring ring{};
        if (io_uring_queue_init(Q_DEPTH, &ring, 0) < 0)
        {
            std::println(stderr, "Failure to init queue!");
            exit(1);
        }

        int inputFD = open(hostname, O_RDONLY | O_NONBLOCK);
        if (inputFD < 0)
        {
            std::println(stderr, "Error opening file: {}", hostname);
            exit(1);
        }

        Request *read_pipe = new Request{.type = OpType::Read, .buffer = new Buffer(), .fd = inputFD, .bytes_to_write = 0};
        submit_read(&ring, inputFD, read_pipe);

        while (true)
        {
            io_uring_cqe *cqe;

            if (io_uring_wait_cqe(&ring, &cqe) < 0)
            {
                std::println(stderr, "wait_cqe failed");
                break;
            }

            Request *data = (Request *)io_uring_cqe_get_data(cqe);
            int res = cqe->res;
            io_uring_cqe_seen(&ring, cqe);

            if (res < 0)
            {
                if (res == -EAGAIN)
                {
                    submit_read(&ring, inputFD, data);
                    continue;
                }
                std::println(stderr, "CQE Error: {}", res);
                break;
            }

            if (data->type == OpType::Read)
            {
                if (res == 0)
                    break; // EOF

                data->buffer->writes_remaining = outputFDs.size();

                for (int outFD : outputFDs)
                {
                    Request *write_req = new Request{
                        .type = OpType::Write,
                        .buffer = data->buffer,
                        .fd = outFD,
                        .bytes_to_write = static_cast<size_t>(res)};

                    submit_write(&ring, outFD, write_req);
                }

                if (io_uring_submit(&ring) < 0)
                {
                    std::println(stderr, "sumbit for read failed");
                }

                delete data;
            }
            else if (data->type == OpType::Write)
            {
                Buffer *shared_buf = data->buffer;
                delete data;

                shared_buf->writes_remaining--;

                if (shared_buf->writes_remaining == 0)
                {
                    Request *next_read = new Request{
                        .type = OpType::Read,
                        .buffer = shared_buf,
                        .fd = inputFD,
                        .bytes_to_write = 0};
                    submit_read(&ring, inputFD, next_read);
                }
            }
        }
    }
}
#endif // BROADCASTER_H