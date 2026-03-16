#include <unistd.h> // for optind
#include <fcntl.h>
#include <print>
#include <vector>
#include <string>
#include <span>
#include <csignal> // For SIGPIPE
#include "Network/broadcastTee.hpp"
#include "file_system/IO_Handle.hpp"
#include <cerrno>
#include <system_error>
#include <filesystem>
#include "file_system/Manage_Pipe.hpp"
#include <fstream>

int get_system_pipe_limit(int requested_size = 1024 * 1024)
{
    std::ifstream file("/proc/sys/fs/pipe-max-size");
    int system_max;
    if (file >> system_max)
    {
        return (requested_size < system_max) ? requested_size : system_max;
    }
    return 65536;
}

int main(int argc, char **argv)
{
    if (argc < 4)
    {
        std::println(stderr, "Broadcaster usage: {} <mode> <input_file> <num_output>", argv[0]);
        return EINVAL;
    }

    signal(SIGPIPE, SIG_IGN);

    int source = open(argv[2], O_RDONLY);
    if (source < 0)
    {
        std::println(stderr, "Error opening file: {}", argv[2]);
        return EBADFD;
    }

    const int MODE = atoi(argv[1]);
    const int COPIES = atoi(argv[3]);

    std::string dir = std::format("./resource/{}/{}", MODE, (COPIES == 1) ? "Single" : (COPIES == 2) ? "Dual"
                                                                                                     : "Multi");
    std::filesystem::create_directories(std::format("{}", dir));

    IO_Handle source_fd(source);
    std::vector<IO_Handle> destination_fds;
    destination_fds.reserve(COPIES);

    for (int i = 0; i < COPIES; i++)
    {
        std::string name = std::format("{}/output_{}.txt", dir, i);
        int fd = open(name.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (fd < 0)
        {
            std::println(stderr, "Warning: No reader on {}, error: {}", name, errno);
            exit(EIO);
        }
        IO_Handle file(fd);
        file.set_name(name);
        destination_fds.push_back(std::move(file));
    }

    std::span<IO_Handle> dest_files(destination_fds);

    if (MODE > 2)
    {
        try
        {
            int max_allowed = get_system_pipe_limit();

            std::vector<ManagedPipe> pipe_storage;
            pipe_storage.reserve(COPIES + 1);

            pipe_storage.emplace_back(dir, 0);
            auto &in_pipe = pipe_storage.back();

            int fd;
            if ((fd = open(in_pipe.c_str(), O_RDWR | O_NONBLOCK)) < 0)
            {
                std::println(stderr, "Error opening file: {}", in_pipe.c_str());
                return EBADFD;
            }

            IO_Handle input_w(fd);
            IO_Handle input_r(dup(fd));

            fcntl(fd, F_SETPIPE_SZ, max_allowed);

            input_w.set_name(in_pipe.name());
            input_r.set_name(in_pipe.name());

            std::vector<IO_Handle> output_pipes_w;
            output_pipes_w.reserve(COPIES);
            std::vector<IO_Handle> output_pipes_r;
            output_pipes_r.reserve(COPIES);

            for (int i = 0; i < COPIES; i++)
            {
                pipe_storage.emplace_back(dir, i + 1);
                auto &pipe_out = pipe_storage.back();

                if ((fd = open(pipe_out.c_str(), O_RDWR | O_NONBLOCK)) < 0)
                {
                    std::println(stderr, "Error opening file: {}", pipe_out.c_str());
                    return EBADFD;
                }

                IO_Handle file_w(fd);
                IO_Handle file_r(dup(fd));

                fcntl(fd, F_SETPIPE_SZ, max_allowed);

                file_w.set_name(pipe_out.name());
                file_r.set_name(pipe_out.name());

                output_pipes_w.push_back(std::move(file_w));
                output_pipes_r.push_back(std::move(file_r));
            }

            std::span<IO_Handle> dest_write_pipe{output_pipes_w};
            std::span<IO_Handle> dest_read_pipe{output_pipes_r};

            if (MODE == 2)
            {
            }
            else
            {
                Network::broadcastTee(
                    source_fd,
                    input_w, input_r,
                    dest_files,
                    dest_write_pipe, dest_read_pipe);
            }
        }
        catch (std::system_error &e)
        {
            std::println(stderr, "System Error: {}", e.what());
            return EIO;
        }
    }

    return 0;
}
