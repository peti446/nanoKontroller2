#include "Actions/Action_Volume.h"

#include <complex>
#include <iostream>

#include "AudioService.h"

namespace {
    float ConvertVolume(int value, float k = 4) {
        return std::pow(static_cast<float>(value) / 127.0f, k);
    }
}

void Action_Volume::on_midi(int value) {
    m_audioService->SetVolume(m_Node, ::ConvertVolume(value, m_PowOverride));
}

void Action_Volume::Init_Internal(const std::unordered_map<std::string, std::string> &params) {
    if (auto Iterator = params.find("pow"); Iterator != params.end()) {
        try {
            m_PowOverride = std::stof(Iterator->second);
        } catch (const std::exception& e) {
            std::cerr << "Warning: Invalid \"pow\" parameter value \"" << Iterator->second
                      << "\" for Volume action on node \"" << m_Node
                      << "\": " << e.what() << " – using default.\n";
        }
    }
}
