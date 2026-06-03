#include "Actions/ActionFactoryRegistry.h"
#include "Actions/IAction.h"
#include <iostream>

std::unique_ptr<class IActionBase> ActionFactoryRegistry::BuildAction(const std::string &action_name,
                                                                      const std::unordered_map<std::string, std::string> &params,
                                                                      LedManager &ledManager,
                                                                      AudioService& audioService, NanoKontrol2::Control cc) {
    std::string lowerActionName = action_name;
    std::transform(lowerActionName.begin(), lowerActionName.end(), lowerActionName.begin(), ::tolower);
    const auto Itr = m_actionFactory.find(lowerActionName);
    if (Itr == m_actionFactory.end()) {
        std::cerr << "Error: No action registered with type name \"" << action_name << "\"\n";
        return nullptr;
    }

    auto Ptr = Itr->second(params, ledManager, audioService, cc);
    return Ptr;
}

void ActionFactoryRegistry::Register(const std::string &action_name, FactoryFunction factory) {
    std::string lowerActionName = action_name;
    std::transform(lowerActionName.begin(), lowerActionName.end(), lowerActionName.begin(), ::tolower);

    if (m_actionFactory.contains(lowerActionName)) {
        throw std::runtime_error("Action already registered: " + action_name);
    }
    m_actionFactory[lowerActionName] = std::move(factory);
}