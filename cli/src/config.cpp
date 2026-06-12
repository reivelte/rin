// SPDX-FileCopyrightText: (c) rin contributors
//
// SPDX-License-Identifier: GPL-3.0-only

#include <fstream>
#include "config.hpp"

namespace rin
{
    program_configuration read_config(const nlohmann::json& json)
    {
        return program_configuration{
            .database_path = json["default_library"]
        };
    }

    sz::result<nlohmann::json> make_default_config_json(const std::filesystem::path& target_path)
    {
        nlohmann::json json;
        json["default_library"] = target_path.parent_path() / "library.db"; // TODO: the default name for the database should come from a macro we define
        
        // write prettified JSON to file
        std::ofstream o(target_path);
        if (!o)
        {
            return sz::common_error(sz::result_code::Incomplete_Result);
        }

        o << std::setw(4) << json << std::endl;
        o.close();
        return json;
    }
    
    sz::result<nlohmann::json> get_default_config()
    {
        // get the path to the current user's folder
        #ifdef _WIN32
        const char* home = std::getenv("USERPROFILE");
        #else
        const char* home = std::getenv("HOME");
        #endif

        if (!home)
        {
            return sz::common_error(sz::result_code::File_Not_Found);
        }

        #ifdef _WIN32
        std::filesystem::path default_config_location = std::filesystem::path(home) / "AppData/Local/rin/config.json";
        default_config_location = default_config_location.make_preferred();
        #else
        std::filesystem::path default_config_location = std::filesystem::path(home) / ".config/rin/config.json";
        #endif

        if (!std::filesystem::exists(default_config_location.parent_path())) 
        { 
            std::filesystem::create_directories(default_config_location.parent_path()); 
            return make_default_config_json(default_config_location); 
        }
        else if (!std::filesystem::exists(default_config_location))
        {
            return make_default_config_json(default_config_location);
        }

        return sz::utility::json_from_file(default_config_location);
    }
}
