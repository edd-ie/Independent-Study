#pragma once

#include "../util/IO_Handle.hpp"
#include <sys/types.h>
#include <vector>
#include <stdlib.h>
#include <memory>

namespace Network
{
    enum class OpType
    {
        Read,
        Write
    };

    class Request
    {
        OpType type;
        std::shared_ptr<std::vector<uint8_t>> buffer;
        std::shared_ptr<Util::IO_Handle> file;
        size_t bytes_to_write;

    public:
        Request(OpType inType,
                std::shared_ptr<std::vector<uint8_t>> data,
                std::shared_ptr<Util::IO_Handle> fd,
                size_t write_size);

        Request(const Request &) noexcept = default;
        Request &operator=(const Request &) noexcept = default;
        Request(Request &&) noexcept = default;
        Request &operator=(Request &&) noexcept = default;
        ~Request() = default;

        Util::IO_Handle::native_handle_type getFile();
        u_int8_t *getData();
        size_t bytes();
        OpType getType();
        std::shared_ptr<std::vector<uint8_t>> getSharedBuffer();
    };

} // namespace Network