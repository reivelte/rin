// SPDX-FileCopyrightText: (c) rin contributors
//
// SPDX-License-Identifier: GPL-3.0-only

#pragma once
#include <print>
#include <string>
#include <vector>
#include <optional>
#include <argparse/argparse.hpp>

namespace rin::argument_definitions
{
    struct tag_args : public argparse::Args 
    {
        bool& sidecar = flag("s,sidecar", "Specify that the tags for the given file or directory should be loaded from sidecar files.");
        bool& tag_recursed_directories = flag("tag-recursed-directories", "Specify that sub-directories should also be tagged when recursing into a given parent directory.");
        bool& target_is_tag = flag("t,target-is-tag", "Specify that the given input is a tag instead of a file/directory. If the given tag does not exist, it will be registered. Flags related to sidecars are ignored if this flag is used.");
        bool& check_tags = flag("c,check-tag", "Check if the tag exists in the library and skip tags that do not already exist in the library.");

        std::string& sidecar_type = kwarg("sidecar-type", "Specify the type of sidecar file to look for. One of 'text' or 'json' is supported.").set_default("text");
        std::vector<std::string>& sidecar_keys = kwarg("sidecar-keys", "JSON keys to pull tags from.").set_default("tags");
        std::string& sidecar_spliton = kwarg("sidecar-spliton", "Substring to delimit JSON value on.").set_default(" ");
        std::optional<std::string>& sidecar_path_prefix = kwarg("sidecar-path-prefix", "Specify an alternate directory containing sidecar files associated with the file(s) to be tagged.");
        
        std::string& input  = arg("input", "A string to use as a tag (-t required) or a path to file or directory to assign tags for. Tagging a directory does not recurse tags into items inside the directory unless the path ends with '/'.");
        std::optional<std::vector<std::string>>& tags   = arg("tags", "Comma delimited list of tags.").multi_argument();
        
        void welcome() override { std::print("Assign tags to a file, directory, or other tag."); }
    };

    struct search_args : public argparse::Args
    {
        bool& show_tags = flag("s,show-tags", "Also print the full tag list associated with searched files.");
        bool& no_print = flag("n,no-print", "Do not print search results to stdout. -s is ignored if this flag is used.");
        bool& use_aliases = flag("a,use-aliases", "Specify that some or all given tags might be aliases for other tags and that the actual tag that the alias points to should be used for queries.");
        bool& recurse = flag("r,recurse", "Recursively get files when encountering a tag that has child tags.");
        
        //std::vector<std::string>& tags = arg("tags", "Tags to search files of.");
        std::string& query = arg("query", "Search query.");

        std::optional<std::string>& link_base_dir = kwarg("l,link-base-dir", "Specify the root directory that will be used to create symlinks to search results.");

        void welcome() override { std::print("Search for tagged files."); }
    };

    struct edit_args : public argparse::Args
    {
        bool& delete_entity = flag("d,delete", "Delete the specified entity. Note that no warning message will be given before the operation executes. Values specified for 'inputs' are ignored if this flag is used.");
        bool& no_alias = flag("no-alias", "If renaming the entity, this flag specifies that the old entity name should not become an alias for the new name.");

        std::string& id = arg("entity_id", "Identifier of entity in which to edit - either a filesystem path or fully qualified tag name.");
        std::optional<std::vector<std::string>>& inputs = arg("inputs", "Comma delimited list of values to assign to the given entity. Valid values differ depending on the detected entity type. Specify values as 'key=value'. Valid keys include: id, rgba, description.");

        void welcome() override { std::print("Edit entries in the given library."); }
    };

    struct find_args : public argparse::Args
    {
        bool& match_modtime = flag("m,match-modtime", "When cross-referencing with the database, possible matches in the search path must have the same modtime as the entity recorded in the database.");
        bool& match_size = flag("s,match-size", "Same as match-modtime but for filesize.");
        bool& ignore_filename = flag("x,ignore-filename", "Do not try to match based on filenames. This flag implies both -s and -m if neither flag is given.");
        bool& relink = flag("l,link", "Immediately re-link the first occurrence of any possible match.");
        std::string& search_path = arg("search-path", "Path to directory to search for entities in need of re-linking");

        void welcome() override { std::print("Find orphaned files and directories."); }
    };

    struct main_args : public argparse::Args 
    {
        tag_args& tag_cmd       = subcommand("tag");
        search_args& search_cmd = subcommand("search");
        edit_args& edit_cmd     = subcommand("edit");
        find_args& find_cmd     = subcommand("find");
        
        bool& version = flag("version", "Print the version and exit.");
        bool& verbose = flag("v,verbose", "Print additional messages related to the current action taking place.");
        std::optional<std::string>& config_path = kwarg("c,config", "Path to config file. If this is omitted then the usual directories are searched for a config file.");
        std::optional<std::string>& database_path     = kwarg("l,library", "Path to the library database file. Overrides the value for 'default_library' in the config.");

        void welcome() override { std::print("Reflexive Indexer. Index files, directories, and user-defined objects using additional metadata."); }
    };
} // namespace rin::argument_definitions

