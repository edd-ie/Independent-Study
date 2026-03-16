#include "IO_Handle.hpp"
#include <unistd.h>
#include <utility>

IO_Handle::IO_Handle(IO_Handle &&other) noexcept : fd{std::exchange(other.fd, IO_Handle::unassigned_fd)}, file_name{other.file_name}
{
}

IO_Handle &IO_Handle::operator=(IO_Handle &&other) noexcept
{
    if (this != &other)
    {
        if (*this)
            close();
        fd = std::exchange(other.fd, unassigned_fd);
    }
    file_name = std::exchange(other.file_name, default_name);
    return *this;
}

IO_Handle::~IO_Handle() noexcept
{
    if (IO_Handle &self = *this; self)
    {
        this->close();
    }
}

IO_Handle::IO_Handle(native_handle_type val)
{
    this->fd = val;
}

IO_Handle::operator bool() const noexcept
{
    return !(this->fd == unassigned_fd);
}

IO_Handle::native_handle_type IO_Handle::native_handle() noexcept
{
    return fd;
}

void IO_Handle::close()
{
    ::close(fd);
}
