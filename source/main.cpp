#include <unistd.h> // for optind
#include <fcntl.h>
#include <print>
#include <vector>
#include <csignal> // For SIGPIPE
#include "Network/broadcaster.hpp"
#include "Network/broadcastSplice.hpp"
#include "Network/broadcastTee.hpp"

int main(int argc, char **argv)
{
    if (argc < 3)
    {
        std::println(stderr, "Usage: {} <input_fifo> <output_fifo1> [output_fifo2...]", argv[0]);
        return 1;
    }

    // If a reader closes their pipe, prevent broadcaster from crash!
    signal(SIGPIPE, SIG_IGN);

    std::vector<std::shared_ptr<Util::IO_Handle>> outputFDs;
    outputFDs.reserve(argc - 2);

    int source = open(argv[1], O_RDONLY);
    if (source < 0)
    {
        std::println(stderr, "Error opening file: {}", argv[1]);
        exit(1);
    }

    auto inputFD = std::make_shared<Util::IO_Handle>(source);

    for (int i = 2; i < argc; i++)
    {
        int fd = open(argv[i], O_WRONLY | O_NONBLOCK);
        if (fd < 0)
        {
            std::println(stderr, "Warning: No reader on {}, error: {}", argv[i], errno);
            continue;
        }
        outputFDs.push_back(std::make_shared<Util::IO_Handle>(fd));
    }

    if (outputFDs.empty())
    {
        std::println(stderr, "Error: No output pipes could be opened.");
        return 1;
    }

    // Network::broadcast(inputFD, outputFDs);
    // Network::broadcastSplice(inputFD, outputFDs);
    Network::broadcastTee(inputFD, outputFDs);

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