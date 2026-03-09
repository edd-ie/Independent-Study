#include "Request.hpp"

namespace Network
{
    Request::Request(OpType inType, std::shared_ptr<std::vector<uint8_t>> data, std::shared_ptr<Util::IO_Handle> fd, size_t write_size) : type(inType), buffer(std::move(data)), file(fd), bytes_to_write(write_size) {}

    Util::IO_Handle::native_handle_type Request::getFile()
    {
        return file.get()->native_handle();
    }

    u_int8_t *Request::getData()
    {
        return buffer->data();
    }

    size_t Request::bytes()
    {
        return bytes_to_write;
    }

    OpType Request::getType()
    {
        return type;
    }

    std::shared_ptr<std::vector<uint8_t>> Request::getSharedBuffer()
    {
        return buffer;
    }

} // namespace Network
