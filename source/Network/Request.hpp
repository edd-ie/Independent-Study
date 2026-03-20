#pragma once

#include "../file_system/IO_Handle.hpp"
#include <sys/types.h>
#include <vector>
#include <string>
#include <stdlib.h>
#include <memory>

class Request
{
    OpType type;
    std::vector<uint8_t> buffer;
    IO_Handle &file;
    size_t bytes_to_write;

public:
    Request(OpType inType,
            std::vector<uint8_t> data,
            IO_Handle &fd,
            size_t write_size);

    Request(const Request &) noexcept = default;
    Request &operator=(const Request &) noexcept = default;
    Request(Request &&) noexcept = default;
    Request &operator=(Request &&) noexcept = default;
    ~Request() = default;
    std::string get_name() { return file.get_name(); }

    IO_Handle::native_handle_type getFile();
    std::vector<u_int8_t> getData();
    size_t bytes();
    OpType getType();
    void setType(OpType);
    std::vector<uint8_t> getSharedBuffer();
};
