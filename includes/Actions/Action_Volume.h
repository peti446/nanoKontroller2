#pragma once
#include "IAction.h"

class Action_Volume final : public IAction<Action_Volume, "volume"> {
public:
    void on_midi(int value) override;

protected:
    void Init_Internal(const std::unordered_map<std::string, std::string> &params) override;
    float m_PowOverride = 4.0f;
};
