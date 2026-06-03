#pragma once
#include <functional>
#include <memory>

#include "NanoKontrol2.hpp"

class AudioService;
class LedManager;
class IActionBase;

class ActionFactoryRegistry {
    typedef std::function<std::unique_ptr<IActionBase>(const std::unordered_map<std::string, std::string>&, LedManager&, AudioService&, const NanoKontrol2::Control)> FactoryFunction;
public:
    static std::unique_ptr<IActionBase> BuildAction(const std::string& action_name,
                                                    const std::unordered_map<std::string, std::string> &params,
                                                    LedManager& ledManager,
                                                    AudioService& audioService, NanoKontrol2::Control cc);
    static void Register(const std::string& action_name, FactoryFunction factory);

    ActionFactoryRegistry(const ActionFactoryRegistry&) = delete;
    ActionFactoryRegistry& operator=(const ActionFactoryRegistry&) = delete;
    ActionFactoryRegistry(ActionFactoryRegistry&&) = delete;
    ActionFactoryRegistry& operator=(ActionFactoryRegistry&&) = delete;
private:
    ActionFactoryRegistry() = default;
    ~ActionFactoryRegistry() = default;

    inline static std::unordered_map<std::string, FactoryFunction> m_actionFactory;
};