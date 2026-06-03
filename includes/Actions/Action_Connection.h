#pragma once
#include "IAction.h"


class Action_Connection : public IAction<Action_Connection, "connection"> {
public:
    void on_midi(int value) override;

    void Refresh_LED_State();

    void on_node_available(WpNode* node, const std::string& name) override;
    void on_link_created(WpLink *link, const std::string &leftNodeName, const std::string &rightNodeName) override;
    void on_link_removed(WpLink *link, const std::string &leftNodeName, const std::string &rightNodeName) override;
    void on_audio_system_ready() override;

protected:
    void Init_Internal(const std::unordered_map<std::string, std::string> &params) override;

    std::string m_Left;
    std::string m_Right;
    bool m_bIsConnected{false};
    bool m_bWaitingForLow{false};
    bool m_bDefaultState{false};
};