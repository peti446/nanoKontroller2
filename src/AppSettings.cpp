#include "AppSettings.h"
#include <glaze/glaze.hpp>
#include <iostream>

AppSettings AppSettingsManager::GetAppSettings(const std::string &configFile) {
    AppSettings Settings;
    std::string Buffer;
    const auto Error = glz::read_file_json(Settings, configFile, Buffer);
    if (Error) {
        std::cerr << "Failed to load config file \"" << configFile << "\": "
                  << glz::format_error(Error, Buffer) << '\n';
    } else {
        std::cout << "Config loaded from \"" << configFile << "\"\n";
    }
    return Settings;
}