#include "IO_Handle.hpp"

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

off_t IO_Handle::get_file_size(native_handle_type fd)
{
    struct stat st;
    if (fstat(fd, &st) < 0)
    {
        perror("fstat");
        return -1;
    }
    if (S_ISBLK(st.st_mode))
    {
        unsigned long long bytes;
        if (ioctl(fd, BLKGETSIZE64, &bytes) != 0)
        {
            perror("ioctl");
            return -1;
        }
        return bytes;
    }
    else if (S_ISREG(st.st_mode))
        return st.st_size;
    return -1;
}