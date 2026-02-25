#include <unistd.h> // for optind
#include <fcntl.h>
#include <print>
#include <vector>
#include <csignal> // For SIGPIPE
#include "Network/broadcaster.h"

int main(int argc, char **argv)
{
    if (argc < 3)
    {
        std::println(stderr, "Usage: {} <input_fifo> <output_fifo1> [output_fifo2...]", argv[0]);
        return 1;
    }

    // If a reader closes their pipe, prevent broadcaster from crash!
    signal(SIGPIPE, SIG_IGN);

    char *const host = argv[1];
    std::vector<int> clients;
    clients.reserve(argc - 2);

    for (int i = 2; i < argc; i++)
    {
        int fd = open(argv[i], O_WRONLY | O_NONBLOCK);
        if (fd < 0)
        {
            // fails with ENXIO, means no one is reading the pipe yet.
            std::println(stderr, "Warning: No reader on {}, error: {}", argv[i], errno);
            continue;
        }
        clients.push_back(fd);
    }

    if (clients.empty())
    {
        std::println(stderr, "Error: No output pipes could be opened.");
        return 1;
    }

    broadcaster::broadcast(host, clients);

    return 0;
}

// void runIO(int argc, char **argv)
// {
//     // --- 1. Test readv() Implementation ---
//     std::println("\n>>> STARTING READV TEST");
//     optind = 1; // RESET getopt pointer
//     readv_impl::readV_cat(argc, argv);

//     // --- 2. Test io_uring Implementation ---
//     std::println("\n>>> STARTING IO_URING TEST");
//     optind = 1;
//     // RESET getopt pointer
//     uring_impl::uring_cat(argc, argv);

//     // --- 3. Test io_uring_batch Implementation ---
//     std::println("\n>>> STARTING IO_URING BATCH-PROCESSING TEST");
//     optind = 1;
//     // RESET getopt pointer
//     uring_batch_impl::uring_batch_cat(argc, argv);

//     // --- 4. Test io_uring_batch Implementation ---
//     std::println("\n>>> STARTING IO_URING ASYNC BATCH-PROCESSING TEST");
//     optind = 1;
//     // TODO: fix infinite loop
//     // RESET getopt pointer
//     // uring_batch_async_impl::uring_batch_async_cat(argc, argv);
// }