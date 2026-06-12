// SPDX-FileCopyrightText: (c) rin contributors
//
// SPDX-License-Identifier: GPL-3.0-only

#include <botan/hash.h>
#include <botan/hex.h>
#include "hash.hpp"
#include "files.hpp"

namespace sz::utility
{
    std::string sha256(std::string_view in)
    {
        auto hash = Botan::HashFunction::create("SHA-256");
        hash->update(in);
        return Botan::hex_encode(hash->final(), false);
    }

    std::string sha256_from_stat(const std::filesystem::path& path)
    {
        std::string hash;
        if (result<file_stat> s = stat(path); s)
        {
            auto path_str = path.filename().string();
            auto size = s->size;
            auto modtime = s->modtime;
            auto data = std::format("{}{}{}", path_str, size, modtime);
            hash = sha256(data);
        }
        return hash;
    }
}
