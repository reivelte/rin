// SPDX-FileCopyrightText: (c) rin contributors
//
// SPDX-License-Identifier: GPL-3.0-only

#include <chrono>
#include "files.hpp"

namespace sz::utility
{
    result<file_stat> stat(const std::filesystem::path& path)
    {
        std::error_code ec;
        auto ftime = std::filesystem::last_write_time(path, ec);
        std::uintmax_t filesize = std::filesystem::file_size(path, ec);
        if (ec)
        { return common_error(ec); }

        #if defined(__clang__)
        int64_t ftime_as_unix_timestamp = std::chrono::duration_cast<std::chrono::seconds>(ftime.time_since_epoch()).count(); // utc on MacOS, Windows is TODO
        #elif defined(__GNUG__)
        int64_t ftime_as_unix_timestamp = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::clock_cast<std::chrono::utc_clock>(ftime).time_since_epoch()).count();
        #elif defined(_MSC_VER)
        // TODO
        int64_t ftime_as_unix_timestamp = std::chrono::duration_cast<std::chrono::seconds>(ftime.time_since_epoch()).count();
        #endif
        
        return file_stat(ftime_as_unix_timestamp, filesize);
    }
}
