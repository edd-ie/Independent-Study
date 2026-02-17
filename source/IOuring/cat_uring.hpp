//
// Created by _edd.ie_ on 16/02/2026.
//

#ifndef CAT_URING_H
#define CAT_URING_H

/*
 * Place as many request as the queue length will allow.
 * These operations can be a mix of reads, writes, etc.
 * Then, call the io_uring_enter() system call to tell
 * the kernel requests are added to the submission queue.
 * Once the kernel's done, CQEs can be accessed from user space.
 */

#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/syscall.h>
#include <sys/mman.h>
#include <sys/uio.h>
#include <linux/fs.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <linux/io_uring.h>
#include <cstdint>

#define QUEUE_DEPTH 1
#define BLOCK_SZ 1024

/* x86 specific */
#define read_barrier() __asm__ __volatile__("" ::: "memory")
#define write_barrier() __asm__ __volatile__("" ::: "memory")

struct app_io_sq_ring
{
    unsigned *head;
    unsigned *tail;
    unsigned *ring_mask;
    unsigned *ring_entries;
    unsigned *flags;
    unsigned *array;
};

struct app_io_cq_ring
{
    unsigned *head;
    unsigned *tail;
    unsigned *ring_mask;
    unsigned *ring_entries;
    struct io_uring_cqe *cqes;
};

struct submitter
{
    int ring_fd;
    struct app_io_sq_ring sq_ring;
    struct io_uring_sqe *sqes;
    struct app_io_cq_ring cq_ring;
};

struct file_info
{
    off_t file_sz;
    struct iovec iovecs[]; /* Referred by readv/writev */
};

int io_uring_setup(unsigned entries, struct io_uring_params *p)
{
    return static_cast<int>(syscall(__NR_io_uring_setup, entries, p));
}

int io_uring_enter(int ring_fd, unsigned int to_submit,
                   unsigned int min_complete, unsigned int flags)
{
    return static_cast<int>(syscall(__NR_io_uring_enter, ring_fd, to_submit, min_complete,
                                    flags, NULL, 0));
}

/*
 * Returns the size of the file whose file descriptor is passed in.
 * Handles regular file and block devices as well.
 */
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

/*
 * IO_Uring setup
 */

int app_setup_uring(struct submitter *s)
{
    struct app_io_sq_ring *sring = &s->sq_ring;
    struct app_io_cq_ring *cring = &s->cq_ring;
    struct io_uring_params p{};
    void *sq_ptr = nullptr;
    void *cq_ptr = nullptr;
    /*
     * Pass in the io_uring_params structure to the io_uring_setup()
     * call zeroed out.
     * Set any flags if we need to, this one doesn't
     * */
    // memset(&p, 0, sizeof(p)); C
    s->ring_fd = io_uring_setup(QUEUE_DEPTH, &p);
    if (s->ring_fd < 0)
    {
        perror("Error: io_uring_setup failed");
        return 1;
    }

    /*
     * io_uring communication happens via 2 shared kernel-user space ring buffers,
     * which can be jointly mapped with a single mmap() call in recent kernels.
     * While the completion queue is directly manipulated, the submission queue
     * has an indirection array in between.
     * */
    int sring_sz = p.sq_off.array + p.sq_entries * sizeof(unsigned);
    int cring_sz = p.cq_off.cqes + p.cq_entries * sizeof(struct io_uring_cqe);

    /*
     * In kernel version 5.4 and above, it is possible to map the submission and
     * completion buffers with a single mmap() call.
     * Rather than check for kernel versions,
     * the recommended way is to just check the features field of the
     * io_uring_params structure, which is a bit mask.
     * If the IORING_FEAT_SINGLE_MMAP is set, then
     * can do away with the second mmap() call
     * to map the completion ring.
     * */
    if (p.features & IORING_FEAT_SINGLE_MMAP)
    {
        if (cring_sz > sring_sz)
        {
            sring_sz = cring_sz;
        }
        cring_sz = sring_sz;
    }

    /*
     * Map in the submission and completion queue ring buffers.
     * Older kernels only map in the submission queue.
     * */
    sq_ptr = mmap(0, sring_sz, PROT_READ | PROT_WRITE,
                  MAP_SHARED | MAP_POPULATE, s->ring_fd,
                  IORING_OFF_SQ_RING);

    if (sq_ptr == MAP_FAILED)
    {
        perror("Error: SQ mmap allocation failed");
        return 1;
    }

    if (p.features & IORING_FEAT_SINGLE_MMAP)
    {
        cq_ptr = sq_ptr;
    }
    else
    {
        /* Map in the completion queue ring buffer in older kernels separately */
        cq_ptr = mmap(0, cring_sz, PROT_READ | PROT_WRITE,
                      MAP_SHARED | MAP_POPULATE, s->ring_fd,
                      IORING_OFF_CQ_RING);

        if (cq_ptr == MAP_FAILED)
        {
            perror("Error: CQ mmap allocation failed");
            return 1;
        }
    }

    /*
     * Save useful fields in a global app_io_sq_ring struct
     * for later easy reference
     */
    // sring->head = sq_ptr + p.sq_off.head;
    // sring->tail = sq_ptr + p.sq_off.tail;
    // sring->ring_mask = sq_ptr + p.sq_off.ring_mask;
    // sring->ring_entries = sq_ptr + p.sq_off.ring_entries;
    // sring->flags = sq_ptr + p.sq_off.flags;
    // sring->array = sq_ptr + p.sq_off.array;

    /*
     * static_cast<char*>(sq_ptr): This tells C++,
     *   "Treat this address as a byte array
     *   so I can add an offset in bytes."
     * reinterpret_cast<unsigned*>: This tells C++,
     *   "Now that you've found the memory address,
     *   treat the data at the address as an unsigned* ."
     */

    sring->head = reinterpret_cast<unsigned *>(static_cast<char *>(sq_ptr) + p.sq_off.head);
    sring->tail = reinterpret_cast<unsigned *>(static_cast<char *>(sq_ptr) + p.sq_off.tail);
    sring->ring_mask = reinterpret_cast<unsigned *>(static_cast<char *>(sq_ptr) + p.sq_off.ring_mask);
    sring->ring_entries = reinterpret_cast<unsigned *>(static_cast<char *>(sq_ptr) + p.sq_off.ring_entries);
    sring->flags = reinterpret_cast<unsigned *>(static_cast<char *>(sq_ptr) + p.sq_off.flags);
    sring->array = reinterpret_cast<unsigned *>(static_cast<char *>(sq_ptr) + p.sq_off.array);

    /* Map in the submission queue entries array */
    void *sqes_map = mmap(0, p.sq_entries * sizeof(struct io_uring_sqe),
                          PROT_READ | PROT_WRITE, MAP_SHARED | MAP_POPULATE,
                          s->ring_fd, IORING_OFF_SQES);

    if (sqes_map == MAP_FAILED)
    {
        perror("Error: SQE mmap failed");
        return 1;
    }

    s->sqes = static_cast<struct io_uring_sqe *>(sqes_map);

    /*
     * Save useful fields in a global app_io_cq_ring struct
     */
    // cring->head = cq_ptr + p.cq_off.head;
    // cring->tail = cq_ptr + p.cq_off.tail;
    // cring->ring_mask = cq_ptr + p.cq_off.ring_mask;
    // cring->ring_entries = cq_ptr + p.cq_off.ring_entries;
    // cring->cqes = cq_ptr + p.cq_off.cqes;
    cring->head = reinterpret_cast<unsigned *>(static_cast<char *>(cq_ptr) + p.cq_off.head);
    cring->tail = reinterpret_cast<unsigned *>(static_cast<char *>(cq_ptr) + p.cq_off.tail);
    cring->ring_mask = reinterpret_cast<unsigned *>(static_cast<char *>(cq_ptr) + p.cq_off.ring_mask);
    cring->ring_entries = reinterpret_cast<unsigned *>(static_cast<char *>(cq_ptr) + p.cq_off.ring_entries);
    cring->cqes = reinterpret_cast<struct io_uring_cqe *>(static_cast<char *>(cq_ptr) + p.cq_off.cqes);

    return 0;
}

/*
 * Output a string of characters of len length to stdout.
 * Buffered output for efficient,
 * As it needs to output character-by-character.
 * */
void output_to_console(char *buf, int len)
{
    while (len--)
    {
        fputc(*buf++, stdout);
    }
}

/*
 * Read completion events from completion queue.
 * Get the data buffer that will have the file data
 * Print it to the console.
 * */
void read_from_cq(struct submitter *s)
{
    struct file_info *fi;
    struct app_io_cq_ring *cring = &s->cq_ring;
    struct io_uring_cqe *cqe;
    unsigned head = *cring->head;

    while (true)
    {
        read_barrier();
        /*
         * This is a ring buffer.
         * If head == tail then:
         *  buffer is empty.
         * */
        if (head == *cring->tail)
            break;

        /* Get the entry */
        cqe = &cring->cqes[head & *cring->ring_mask];
        fi = reinterpret_cast<struct file_info *>(static_cast<uintptr_t>(cqe->user_data));

        if (cqe->res < 0)
            fprintf(stderr, "Async I/O Error: %s\n", strerror(abs(cqe->res)));
        else
        {
            int blocks = static_cast<int>(fi->file_sz) / BLOCK_SZ;
            if (fi->file_sz % BLOCK_SZ)
                blocks++;
            for (int i = 0; i < blocks; i++)
                output_to_console(static_cast<char *>(fi->iovecs[i].iov_base), fi->iovecs[i].iov_len);
        }
        head++;
    }
    write_barrier();
    *cring->head = head;
}

#endif // CAT_URING_H