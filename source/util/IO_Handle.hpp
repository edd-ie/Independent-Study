#pragma once

#include <type_traits>

namespace Util
{
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

    private:
        static constexpr native_handle_type unassigned_fd{-1};
        native_handle_type fd{unassigned_fd};

        static_assert(std::is_trivial_v<native_handle_type>);
    };

} // namespace Handler
