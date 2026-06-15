// SPDX-FileCopyrightText: (c) rin contributors
//
// SPDX-License-Identifier: GPL-3.0-only

#include <print>
#include <filesystem>
#include <argparse/argparse.hpp>
#include <suzuri/entity/database.hpp>
#include <suzuri/metadata/tags.hpp>
#include <suzuri/utility/files.hpp>
#include <suzuri/utility/hash.hpp>
#include <suzuri/utility/json.hpp>
#include <suzuri/utility/string.hpp>
#include <suzuri/utility/domain.hpp>
#include "args.hpp"
#include "config.hpp"

namespace rin
{
    using namespace rin::argument_definitions;

    sz::result_code tag_input(const tag_args& args, sz::entity_database& db, bool verbose)
    {
        auto return_code = sz::result_code::Success;
        if (!args.target_is_tag)
        {
            std::vector<std::string> tags;
            std::vector<std::filesystem::path> sidecars;
            std::vector<std::filesystem::directory_entry> entries;
            if (std::filesystem::is_directory(args.input) && (std::string_view(args.input).back() == '/')) 
            { entries.append_range(std::filesystem::recursive_directory_iterator(args.input, std::filesystem::directory_options::skip_permission_denied)); } 
            else 
            { entries.emplace_back(args.input); }
            
            for (const std::filesystem::directory_entry& entry : entries)
            {
                std::string suffix = entry.path().filename().string();
                if (entry.is_directory() && !args.tag_recursed_directories && (entries.size() != 1))
                { continue; }
                if (args.sidecar)
                {
                    if (std::filesystem::path sidecar_path( 
                        (args.sidecar_path_prefix.has_value() ? 
                            std::format("{}/{}", sz::utility::trim_r(*args.sidecar_path_prefix, '/'), suffix) : 
                            entry.path().string()) + std::format(".{}", args.sidecar_type) ); 
                        std::filesystem::exists(sidecar_path))
                    {
                        tags.append_range(sz::metadata::get_tags(sidecar_path, args.sidecar_keys, args.sidecar_spliton));
                        if (!args.sidecar_path_prefix.has_value() && args.tags.has_value())
                        { sidecars.emplace_back(std::move(sidecar_path)); }
                    }
                }
                if (args.tags.has_value())
                { tags.append_range(args.tags.value()); }

                int tags_applied = db.tag(entry.path(), tags, "", 255, false, args.check_tags);
                if (std::cmp_not_equal(tags_applied, tags.size()))
                {
                    if (!entry.exists())
                    { std::cerr << std::format("'{}' not found. Not tagging it.\n", entry.path().string()); }
                    else
                    { std::cerr << "Warning: some tags had invalid characters (one of: , ~ or |) and could not be used to tag the given file(s)\n"; }
                    return_code = sz::result_code::Incomplete_Result;
                }
                if (verbose && tags_applied) 
                { std::println("{} <- {} tag(s) added", suffix, tags_applied); }
                tags.clear();
            }

            for (const std::filesystem::path& sidecar : sidecars)
            { db.delete_entity(sidecar.string()); }

            if (verbose && args.sidecar && sidecars.size())
            { std::println("Removed {} sidecar(s) from the library", sidecars.size()); }
        }
        else
        {
            auto target_tag = std::string(sz::utility::trim(args.input));
            bool target_tag_registered = false;
            if (!db.entity_exists(target_tag))
            {
                if (args.check_tags)
                {
                    std::cerr << std::format("Cannot act on '{}'. Tag not found in library.\n", target_tag);
                    return sz::result_code::Invalid_Argument;
                }
                
                sz::result_code status = db.register_entity(sz::entity_database_info{
                    .id = target_tag
                });

                if (status == sz::result_code::Invalid_Argument)
                {
                    std::cerr << std::format("Failed to register new tag. Invalid characters used in tag string: '{}'\n", target_tag);
                    return status;
                }
                target_tag_registered = true;
            }
            if (!args.tags)
            {
                if (target_tag_registered)
                { return sz::result_code::Success; }

                return sz::result_code::No_Action;
            }
            for (std::string& tag : *args.tags)
            {
                tag = std::string(sz::utility::trim(tag));
                if (!db.entity_exists(tag))
                {
                    if (args.check_tags)
                    {
                        if (verbose)
                        { std::cerr << std::format("'{}' does not exist in the library. Not using this tag.\n", tag); }
                        continue;
                    }
                    
                    sz::result_code status = db.register_entity(sz::entity_database_info{
                        .id = tag
                    });

                    if (status == sz::result_code::Invalid_Argument)
                    {
                        std::cerr << std::format("Tag '{}' has invalid characters and will be ignored.\n", tag);
                        return_code = sz::result_code::Invalid_Argument;
                        continue;
                    }
                }
                if (!db.make_link(target_tag, tag))
                {
                    std::cerr << std::format("Failed to make link from '{}' to '{}'. Making this link would produce a cycle in the link tree.\n", target_tag, tag);
                    return_code = sz::result_code::Invalid_Argument;
                    continue;
                }
            }
        }
        return return_code;
    }

    sz::result_code get_files(const search_args& args, sz::entity_database& db, bool verbose)
    {
        auto files = db.files(sz::entity_database_query{.string = args.query});
        int symlinks_created = 0;

        while (++files)
        {
            auto path = files.get<std::filesystem::path>();
            if (args.link_base_dir.has_value())
            {
                std::error_code ec;
                
                std::filesystem::path link_path = std::filesystem::path(
                    args.link_base_dir.value()) / std::format("{}_{}", sz::utility::sha256(path.string()), path.filename().string()
                );

                std::filesystem::create_directory_symlink(path, link_path, ec);
                if (ec) 
                {
                    if (!std::filesystem::exists(link_path))
                    {
                        sz::check_error(sz::common_error(ec));
                        return sz::result_code::Incomplete_Result;
                    }
                }
                else
                { symlinks_created++; }
            }
            if (!args.no_print)
            {
                std::string fmt = std::format("{}", path.string());
                if (args.show_tags)
                {
                    auto tags = db.tags(path);
                    fmt += std::format(" {}", tags);
                }
                if (!std::filesystem::exists(path))
                { fmt = "*" + fmt; }

                std::cout << fmt << '\n';
            }
        }
        if (symlinks_created && verbose)
        { std::println("{} symlink(s) created.", symlinks_created); }

        return sz::result_code::Success;
    }

    sz::result_code edit_entity(const edit_args& args, sz::entity_database& db)
    {
        if (!db.entity_exists(args.id))
        {
            std::println("{} does not refer to an entity in the library.", args.id);
            return sz::result_code::Invalid_Argument;
        }
        if (args.delete_entity)
        {
            db.delete_entity(args.id);
            return sz::result_code::Success;
        }
        if (args.inputs.has_value())
        {   
            const std::vector<std::string>& kvs = args.inputs.value();
            sz::indexed_result_code pr = db.update_entity(std::string(sz::utility::trim(args.id)), kvs, !args.no_alias);
            
            if (pr.code == sz::result_code::Invalid_Argument) 
            { std::println("Invalid input: {}", kvs[pr.index]); }
            else if (pr.code == sz::result_code::Unknown_Key) 
            { std::println("Unknown key: {}", kvs[pr.index]); }

            return pr.code;
        }
        return sz::result_code::No_Action;
    }

    sz::result_code find_file(const find_args& args, sz::entity_database& db)
    {
        // search_path assumed to be a directory that doesn't have any registered entities
        // 3 attributes to look for: filename, modtime, size
        for (const std::filesystem::directory_entry& entry : std::filesystem::recursive_directory_iterator(args.search_path))
        {
            std::vector<std::string> files;
            auto entry_filename = entry.path().filename().string();

            if (!args.ignore_filename && !args.match_modtime && !args.match_size)
            { files = db.files(entry_filename); }
            else
            {
                if (sz::result<sz::utility::file_stat> st = sz::utility::stat(entry.path()); st)
                {
                    if (args.ignore_filename)
                    {
                        if (args.match_modtime && args.match_size)
                        { files = db.files("", st->modtime, st->size); }
                        else if (args.match_size)
                        { files = db.files("", 0, st->size); }
                        else if (args.match_modtime)
                        { files = db.files("", st->modtime); }
                        else
                        { files = db.files("", st->modtime, st->size); }
                    }
                    else if (args.match_modtime && args.match_size)
                    { files = db.files(entry_filename, st->modtime, st->size); }
                    else if (args.match_modtime)
                    { files = db.files(entry_filename, st->modtime); }
                    else
                    { files = db.files(entry_filename, 0, st->size); }
                }
                else
                {
                    // TODO: handle matching of directories
                    std::cerr << std::format("could not stat '{}': {}\n", entry.path().string(), st.error());
                    continue;
                }
            }
            for (const auto& path : files)
            {
                if (!std::filesystem::exists(path))
                {
                    // entity was moved on the filesystem
                    auto entry_path_str = entry.path().string();
                    if (args.relink)
                    {
                        db.update_entity(path, {std::format("id={}", entry_path_str)}, false);
                        std::println("{} (<- relinked from) {}", entry_path_str, path);
                        break;
                    }
                    else
                    { std::println("{} (possible match with) {}", entry_path_str, path); }
                }
            }
        }
        return sz::result_code::Success;
    }
} // namespace rin

int main(int argc, char** argv)
{
    auto args = argparse::parse<rin::argument_definitions::main_args>(argc, argv);
    sz::result_code exit_code = sz::result_code::No_Action;
    if (args.version)
    {
        std::println("libsuzuri: v0.1-dev\nrin: v0.1-dev"); // TODO
        exit_code = sz::result_code::Success;
    }
    else
    {        
        sz::toml_config conf;
        if (args.config_path)
        {
            auto ret = sz::toml_config::create_or_open(*args.config_path, sz::config_type::Static);
            if (ret)
            { conf = *ret; }
            else
            { return sz::check_error(ret); }
        }
        else
        {
            auto ret = rin::get_default_config();
            if (ret)
            { conf = *ret; }
            else
            { return sz::check_error(ret); }
        }

        const auto conf_default_domain_path = conf.value<std::string>("default_domain");
        std::filesystem::path domain_path = args.domain_path.value_or(conf_default_domain_path);

        if (!std::filesystem::exists(domain_path))
        {
            if (domain_path.empty())
            {
                domain_path = sz::utility::default_domain_path() / "default";
            }

            const auto domain_path_ = domain_path.string();
            
            if (conf_default_domain_path.empty())
            { conf.set_value("default_domain", domain_path_); }
            
            std::println("The domain at {} does not exist. Will attempt to create it.", domain_path_);

            if (!sz::utility::create_domain(domain_path))
            { return std::to_underlying(sz::result_code::Init_Problem); }
        }
        try 
        {
            sz::entity_database db(sz::utility::database_path_from_domain_path(domain_path));
            if (args.tag_cmd.is_valid)
            {
                exit_code = rin::tag_input(args.tag_cmd, db, args.verbose);
            }
            else if (args.search_cmd.is_valid)
            {
                exit_code = rin::get_files(args.search_cmd, db, args.verbose);
            }
            else if (args.edit_cmd.is_valid)
            {
                exit_code = rin::edit_entity(args.edit_cmd, db);
            }
            else if (args.find_cmd.is_valid)
            {
                exit_code = rin::find_file(args.find_cmd, db);
            }
            else
            { args.help(); }
        }
        catch (const sz::exception& e) 
        { 
            std::cerr << std::format("{}\n", e.what()); 
            exit_code = sz::result_code::Exception_Occurred;
        }
    }
    return std::to_underlying(exit_code);
}

