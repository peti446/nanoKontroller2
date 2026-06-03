#pragma once

#include "IAction.h"

class Action_Mute final : public IAction<Action_Mute, "Mute"> {
public:

    void on_midi(int value) override;
    void on_node_available(WpNode *node, const std::string &name) override;
    void on_node_params_changed(WpPipewireObject * node, const std::string &name) override;
    void on_audio_system_ready() override;

protected:
    void Init_Internal(const std::unordered_map<std::string, std::string> &params) override;
private:
    bool m_bIsMuted{false};
    bool m_bWaitingForLow{false};
};