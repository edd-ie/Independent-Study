#ifndef SENDER_PIPE_H
#define SENDER_PIPE_H

#include <sys/types.h>
#include <sys/stat.h>
#include <string>
#include <print>
#include <liburing.h>
#include <assert.h>
#include <vector>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/uio.h>

namespace sender_pipe
{
#define Q_DEPTH 4
#define BLOCK 4096

    void setup_uring(struct io_uring *ring)
    {
        int ret;
        if ((ret = io_uring_queue_init(Q_DEPTH, ring, 0)) < 0)
        {
            std::println(stderr, "Failure to init queue: errno={}", -ret);
            exit(1);
        }
    }

    ssize_t FdGetFileSize(int fd)
    {
        struct stat st{};
        if (fstat(fd, &st) < 0)
            return -1;
        return st.st_size;
    }

    ssize_t GetFileSize(std::string filename)
    {
        struct stat stat_buf;
        int rc = stat(filename.c_str(), &stat_buf);
        return rc == 0 ? stat_buf.st_size : -1;
    }

    void close_uring(struct io_uring *ring)
    {
        io_uring_queue_exit(ring);
    }

    int read_file_request(char *const file_name)
    {
        io_uring ring{};

        setup_uring(&ring);

        int fd = open(file_name, O_RDONLY | O_DIRECT);
        if (fd < 0)
        {
            std::println(stderr, "Error opening file: {}", file_name);
            return 1;
        }

        ssize_t fsize = FdGetFileSize(fd);
        if (fsize < 0)
        {
            std::println(stderr, "Failed to stat file");
            return 1;
        }

        ssize_t aligned_size = (fsize + BLOCK - 1) & ~(BLOCK - 1);
        std::vector<iovec> iovecs(Q_DEPTH);

        for (auto &x : iovecs)
        {
            void *buffer = nullptr;

            if (posix_memalign(&buffer, BLOCK, BLOCK))
            {
                std::println(stderr, "Error Allocating iovecs for reads");
                return 1;
            }
            x.iov_base = buffer;
            x.iov_len = BLOCK;
        }

        off_t offset = 0;
        int idx = 0;
        ssize_t total_read = 0;

        while (offset < aligned_size || idx > 0)
        {
            while (idx < Q_DEPTH && offset < aligned_size)
            {
                io_uring_sqe *sqe = io_uring_get_sqe(&ring);
                if (!sqe)
                {
                    std::println(stderr, "Error getting sqe");
                    break;
                }
                io_uring_prep_readv(sqe, fd, &iovecs[idx], 1, offset);

                // Store buffer index inside user_data
                io_uring_sqe_set_data(sqe, (void *)(uintptr_t)idx);
                offset += BLOCK;
                idx++;
            }

            if (idx == 0)
                break;

            int submit = io_uring_submit(&ring);
            if (submit < 0)
            {
                std::println(stderr, "sumbit for read failed");
            }

            io_uring_cqe *cqe;
            int ret = io_uring_wait_cqe(&ring, &cqe);
            if (ret < 0)
            {
                std::println(stderr, "wait_cqe failed: {}", ret);
            }

            int buf_index = (int)(uintptr_t)io_uring_cqe_get_data(cqe);
            if (cqe->res < 0)
            {
                std::println(stderr, "Read error: {}", -cqe->res);
                io_uring_cqe_seen(&ring, cqe);
                break;
            }

            total_read += cqe->res;
            idx--;

            io_uring_cqe_seen(&ring, cqe);
        }

        std::println("File size: {}", fsize);
        std::println("Bytes read: {}", total_read);

        // Cleanup
        for (auto &b : iovecs)
            free(b.iov_base);

        close(fd);
        io_uring_queue_exit(&ring);

        return 0;
    }

    void send_pipe_data(char *const file)
    {
        // const char *fifoname = "pipe.fifo";

        // if ((mkfifo(fifoname, S_IRWXU)) != 0)
        // {
        //     std::println(stderr, "Unable to create a fifo; errno={}", errno);
        //     exit(1);
        // }

        read_file_request(file);
        // pid_t pid[5];
    }

}
#endif // SENDER_PIPE_H