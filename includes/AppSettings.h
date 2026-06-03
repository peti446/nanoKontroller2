#pragma once
#include <string>
#include <unordered_map>

struct ActionConfig {
    std::string type;
    std::unordered_map<std::string, std::string> params;
};

struct AppSettings {
    std::unordered_map<std::string, std::string> FriendlyNames;
    std::unordered_map<std::string, ActionConfig> Actions;
};

class AppSettingsManager {
public:
    static AppSettings GetAppSettings(const std::string &configFile);
};