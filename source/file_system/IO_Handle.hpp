#pragma once

#include <type_traits>
#include <string>
#include <unistd.h>
#include <utility>
#include <sys/stat.h>
#include <linux/fs.h>
#include <sys/uio.h>
#include <fcntl.h>
#include <sys/ioctl.h>

enum class OpType
{
    Read,
    Write,
    Splice,
    Tee,
    SRC_SPLICE_PIPE,
    PIPE_TEE_PIPE,
    PIPE_SPLICE_PIPE,
    PIPE_SPLICE_DEST
};

class IO_Handle
{
public:
    using native_handle_type = int;

    IO_Handle() noexcept = default;

    IO_Handle(const IO_Handle &) = delete;
    IO_Handle &operator=(const IO_Handle &) = delete;

    IO_Handle(IO_Handle &&) noexcept;
    IO_Handle &operator=(IO_Handle &&) noexcept;

    // Destructor should always be no exception
    ~IO_Handle() noexcept;

    IO_Handle(native_handle_type);

    operator bool() const noexcept;
    [[nodiscard]] native_handle_type native_handle() noexcept;

    void close();

    void set_name(std::string &file) { file_name = file; }
    std::string get_name() { return file_name; }
    static off_t get_file_size(native_handle_type fd);

private:
    static constexpr native_handle_type unassigned_fd{-1};
    std::string default_name = "";
    native_handle_type fd{unassigned_fd};
    std::string &file_name = default_name;

    static_assert(std::is_trivial_v<native_handle_type>);
};
