#include "Request.hpp"

namespace Network
{
    Request::Request(OpType inType, std::vector<uint8_t> data, IO_Handle &fd, size_t write_size) : type(inType), buffer(std::move(data)), file(fd), bytes_to_write(write_size) {}

    IO_Handle::native_handle_type Request::getFile()
    {
        return file;
    }

    std::vector<u_int8_t> Request::getData()
    {
        return buffer;
    }

    size_t Request::bytes()
    {
        return bytes_to_write;
    }

    OpType Request::getType()
    {
        return type;
    }

    std::vector<uint8_t> Request::getSharedBuffer()
    {
        return buffer;
    }

    void Request::setType(OpType new_Type)
    {
        type = new_Type;
    }

} // namespace Network
