#pragma once

#include <string>
#include <cerrno>
#include <sys/stat.h>
#include <system_error>
#include <unistd.h>
#include <format>

class ManagedPipe
{
    std::string path;

public:
    ManagedPipe(std::string &base_path, const int suffix)
    {
        path = std::format("{}_{}.fifo", base_path, suffix);
        if (mkfifo(path.c_str(), 0666) == -1 && errno != EEXIST)
        {
            throw std::system_error(errno, std::generic_category(), "Failed to create pipe");
        }
    }

    ~ManagedPipe()
    {
        unlink(path.c_str());
    }

    const char *c_str() const { return path.c_str(); }
    std::string &name() { return path; }
};