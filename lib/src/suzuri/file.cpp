// SPDX-FileCopyrightText: (c) rin contributors
//
// SPDX-License-Identifier: GPL-3.0-only

#include <chrono>
#include "file.hpp"

namespace sz
{
    file::file() :
        m(new file::data)
    {
    }

    file::file(std::filesystem::path path) :
        m(new file::data{
            .path = path
        })
    {
    }

    file::~file()
    {
    }

    file::operator std::string() const
    {
        return path().string();
    }

    file::operator std::filesystem::path() const
    {
        return path();
    }

    std::filesystem::path file::path() const
    {
        return m->path;
    }

    std::filesystem::path file::name() const
    {
        return path().filename();
    }

    std::filesystem::path file::suffix() const
    {
        return path().extension();
    }

    std::string file::extension() const
    {
        return suffix().string().substr(1);
    }

    result<bool> file::exists() const
    {
        std::error_code ec;
        const bool ret = std::filesystem::exists(m->path, ec);
        return ec ? common_error(ec) : ret;
    }

    result<bool> file::is_file() const
    {
        std::error_code ec;
        const bool ret = std::filesystem::is_regular_file(m->path, ec);
        return ec ? common_error(ec) : ret;
    }

    result<bool> file::is_dir() const
    {
        std::error_code ec;
        const bool ret = std::filesystem::is_directory(m->path, ec);
        return ec ? common_error(ec) : ret;
    }

    result<i64> file::modtime() const
    {
        std::error_code ec;
        auto ftime = std::filesystem::last_write_time(m->path, ec);

        if (ec) {
            return common_error(ec);
        }

        // taken from utility/files.hpp
#if defined(__clang__)
        i64 ftime_as_unix_timestamp = std::chrono::duration_cast<std::chrono::seconds>(ftime.time_since_epoch()).count(); // utc on MacOS, Windows is TODO
#elif defined(__GNUG__)
        i64 ftime_as_unix_timestamp = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::clock_cast<std::chrono::utc_clock>(ftime).time_since_epoch()).count();
#elif defined(_MSC_VER)
        // TODO
        i64 ftime_as_unix_timestamp = std::chrono::duration_cast<std::chrono::seconds>(ftime.time_since_epoch()).count();
#endif
        return ftime_as_unix_timestamp;
    }

} // namespace sz
