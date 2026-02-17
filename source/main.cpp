#include <unistd.h> // for optind

#include <print>

#include "IOuring/cat_readv.hpp"

#include "IOuring/cat_uring.hpp"

#include "IOuring/cat_uring_batch.hpp"

#include "IOuring/cat_uring_batch_async.hpp"

int main(int argc, char **argv)
{

    // --- 1. Test readv() Implementation ---
    std::println("\n>>> STARTING READV TEST");
    optind = 1; // RESET getopt pointer
    readv_impl::readV_cat(argc, argv);

    // --- 2. Test io_uring Implementation ---
    std::println("\n>>> STARTING IO_URING TEST");
    optind = 1;
    // RESET getopt pointer
    uring_impl::uring_cat(argc, argv);

    // --- 3. Test io_uring_batch Implementation ---
    std::println("\n>>> STARTING IO_URING BATCH-PROCESSING TEST");
    optind = 1;
    // RESET getopt pointer
    uring_batch_impl::uring_batch_cat(argc, argv);

    // --- 4. Test io_uring_batch Implementation ---
    std::println("\n>>> STARTING IO_URING ASYNC BATCH-PROCESSING TEST");
    optind = 1;
    // RESET getopt pointer
    uring_batch_async_impl::uring_batch_async_cat(argc, argv);

    return 0;
}