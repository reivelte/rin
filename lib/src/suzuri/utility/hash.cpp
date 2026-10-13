// SPDX-FileCopyrightText: (c) rin contributors
//
// SPDX-License-Identifier: GPL-3.0-only

#include <botan/hash.h>
#include <botan/hex.h>
#include "hash.hpp"
#include "files.hpp"

namespace sz::utility
{
    std::string hash(std::string_view data, hash_type type)
    {
        using enum hash_type;
        std::string_view algo;
        switch (type)
        {
        default:
        case SHA256: { algo = "SHA-256"; break; }
        case MD5: { algo = "MD5"; break; }
        }

        auto h = Botan::HashFunction::create(algo);
        h->update(data);
        return Botan::hex_encode(h->final(), false);
    }

    std::string sha256(std::string_view in)
    {
        return hash(in, hash_type::SHA256);
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
