// SPDX-FileCopyrightText: (c) rin contributors
//
// SPDX-License-Identifier: GPL-3.0-only

#pragma once
#include <optional>
#include <vector>
#include <string>
#include <filesystem>
#include <sz_export.hpp>
#include "../utility/string.hpp"
#include "../entity/database.hpp"

namespace sz::metadata
{
    enum class sidecar_filetype : int
    {
        Plaintext, Json
    };

    struct parameters
    {
        std::vector<std::string> tags;
        std::vector<std::string> sidecar_keys;
        std::string sidecar_split_character;
        std::optional<std::string> sidecar_alternate_root_path;
        std::optional<std::string> sidecar_type;
        bool use_sidecars;
        bool check_tags;
    };
    
    template <typename Func, typename ErrFunc> requires
        is_indicating_function<Func, std::filesystem::path, const std::vector<std::string>&, result_code> && // id, tags, code
        is_indicating_function<ErrFunc, std::string, std::string, result_code> // id, err_msg, code
    SZ_API sz::result_code tag_input(
        std::filesystem::path path,
        sz::entity_database& db,
        const parameters& args,
        Func callback, ErrFunc error_callback,
        bool recursively_tag_directories, bool verbose
    );

    template <typename Func, typename ErrFunc> requires
        is_indicating_function<Func, std::string, result_code> && // id, code
        is_indicating_function<ErrFunc, std::string, std::string, result_code> // id, err_msg, code
    SZ_API sz::result_code tag_input(
        std::string tag,
        sz::entity_database& db,
        const parameters& args,
        Func callback, ErrFunc error_callback,
        bool verbose = false
    );

    SZ_API std::vector<std::string> get_tags(const std::filesystem::path& tagfile_path, const std::vector<std::string>& keys = {}, const std::string& split_on = "");
        
    // TODO: load certain settings, like verbosity level, from a global config set at runtime
    template <typename Func, typename ErrFunc> requires
        is_indicating_function<Func, std::filesystem::path, const std::vector<std::string>&, result_code> &&
        is_indicating_function<ErrFunc, std::string, std::string, result_code>
    sz::result_code tag_input(
        std::filesystem::path path,
        sz::entity_database& db,
        const parameters& args,
        Func callback, ErrFunc error_callback,
        bool tag_encountered_directories, bool verbose
    )
    {
        using enum result_code;
        auto rc = Success;
        std::vector<std::string> tags;
        std::vector<std::filesystem::path> sidecars;
        std::vector<std::filesystem::directory_entry> entries;

        if (std::filesystem::is_directory(path) && (path.string().back() == '/')) 
        { entries.append_range(std::filesystem::recursive_directory_iterator(path, std::filesystem::directory_options::skip_permission_denied)); } 
        else 
        { entries.emplace_back(path); }
        
        for (const std::filesystem::directory_entry& entry : entries)
        {
            const std::string suffix = entry.path().filename().string();
            
            if (entry.is_directory() && !tag_encountered_directories && (entries.size() != 1))
            { continue; }

            if (args.tags.size())
            { tags = args.tags; }
            
            if (args.use_sidecars)
            {
                if (std::filesystem::path sidecar_path( 
                    (args.sidecar_alternate_root_path.has_value() ? 
                        std::format("{}/{}", sz::utility::trim_r(*args.sidecar_alternate_root_path, '/'), suffix) : 
                        entry.path().string()) + std::format(".{}", *args.sidecar_type)
                    ); 
                    std::filesystem::exists(sidecar_path))
                {
                    tags.append_range(sz::metadata::get_tags(sidecar_path, args.sidecar_keys, args.sidecar_split_character));
                    
                    if (!args.sidecar_alternate_root_path && args.tags.size())
                    { sidecars.emplace_back(std::move(sidecar_path)); }
                }
            }

            const int tags_applied = db.tag(entry.path(), tags, "", 255, false, args.check_tags);
            if (std::cmp_not_equal(tags_applied, tags.size()))
            {
                std::string err_msg;
                
                if (!entry.exists())
                { err_msg = std::format("'{}' not found. Not tagging it.\n", entry.path().string()); }
                else
                {
                    // TODO: the list of invalid characters should come from a constant we define somewhere
                    err_msg = "Warning: some tags had invalid characters (one of: , ~ or |) and could not be used to tag the given file(s)\n";
                }
                rc = Incomplete_Result;
                
                if (error_callback(entry.path(), err_msg, rc))
                { return rc; }
            }

            if (callback(entry.path(), tags, rc))
            { return rc; }

            // TODO: move non-error prints to a log function
            if (verbose && tags_applied) 
            { std::println("{} <- {} tag(s) added", suffix, tags_applied); }

            tags.clear();
        }

        for (const std::filesystem::path& sidecar : sidecars)
        { db.delete_entity(sidecar.string()); }

        // TODO: move non-error prints to a log function
        if (verbose && args.use_sidecars && sidecars.size())
        { std::println("Removed {} sidecar(s) from the library", sidecars.size()); }

        return rc;
    }

    template <typename Func, typename ErrFunc> requires
        is_indicating_function<Func, std::string, result_code> && // id, code
        is_indicating_function<ErrFunc, std::string, std::string, result_code> // id, err_msg, code
    SZ_API sz::result_code tag_input(
        std::string tag,
        sz::entity_database &db,
        const parameters &args,
        Func callback, ErrFunc error_callback,
        bool verbose
    )
    {
        using enum result_code;
        result_code rc = Success;
        auto target_tag = std::string(sz::utility::trim(tag));
        bool target_tag_registered = false;
        if (!db.entity_exists(target_tag))
        {
            if (args.check_tags)
            {
                std::string err_msg = std::format("Cannot act on '{}'. Tag not found in library.\n", target_tag);
                rc = Invalid_Argument;
                error_callback(target_tag, err_msg, rc);
                return rc;
            }
            
            sz::result_code status = db.register_entity(sz::entity_database_info{
                .id = target_tag
            });

            if (status == Invalid_Argument)
            {
                std::string err_msg = std::format("Failed to register new tag. Invalid characters used in tag string: '{}'\n", target_tag);
                error_callback(target_tag, err_msg, status);
                return status;
            }
            target_tag_registered = true;
        }
        if (args.tags.empty())
        {
            if (target_tag_registered)
            { return Success; }

            return No_Action;
        }
        for (std::string& tag : args.tags)
        {
            tag = std::string(sz::utility::trim(tag));
            if (!db.entity_exists(tag))
            {
                if (args.check_tags)
                {
                    if (verbose)
                    {
                        std::string err_msg = std::format("'{}' does not exist in the library. Not using this tag.\n", tag);
                        if (error_callback(tag, err_msg, Invalid_Argument))
                        { return Incomplete_Result; }
                    }
                    continue;
                }
                
                sz::result_code status = db.register_entity(sz::entity_database_info{
                    .id = tag
                });

                if (status == Invalid_Argument)
                {
                    std::string err_msg = std::format("Tag '{}' has invalid characters and will be ignored.\n", tag);
                    if (error_callback(tag, err_msg, status))
                    {
                        rc = Incomplete_Result;
                        return rc;
                    }
                    continue;
                }
            }
            if (!db.make_link(target_tag, tag))
            {
                std::string err_msg = std::format("Failed to make link from '{}' to '{}'. Making this link would produce a cycle in the link tree.\n", target_tag, tag);
                if (error_callback(tag, err_msg, Invalid_Argument))
                {
                    rc = Incomplete_Result;
                    return rc;
                }
                continue;
            }
        }
        return rc;
    }
}