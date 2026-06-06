#include "Actions/Action_Connection.h"

#include <iostream>

#include "AudioService.h"
#include "LedManager.h"

void Action_Connection::on_midi(int value) {
    if (value == 127) {
        if (m_bWaitingForLow) {
            return;
        }
        m_bWaitingForLow = true;
        m_bIsConnected = !m_bIsConnected;

        m_audioService->SetConnectionState(m_Left, m_Right, m_bIsConnected);
        Refresh_LED_State();
    } else if (value == 0) {
        m_bWaitingForLow = false;
    }
}

void Action_Connection::Refresh_LED_State() {
    if (LedManager::IsLedCapable(m_Control)) {
        m_ledManager->Set(static_cast<NanoKontrol2::LED>(m_Control), m_bIsConnected);
    }
}

void Action_Connection::on_node_available(WpNode* /*node*/, const std::string& name) {
    on_audio_system_ready();
}

void Action_Connection::on_link_created(WpLink *link, const std::string &leftNodeName,
    const std::string &rightNodeName) {
    // Seed the connection state (and LED) from PipeWire reality once both nodes are present.
    // GetConnectionState returns false if either node is still absent, which is the correct
    // default — we will be called again when the second node appears.
    const bool bConnected = m_audioService->GetConnectionState(m_Left, m_Right);
    if (bConnected != m_bIsConnected) {
        m_bIsConnected = bConnected;
        Refresh_LED_State();
    }
}

void Action_Connection::on_link_removed(WpLink *link, const std::string &leftNodeName,
    const std::string &rightNodeName) {
    // Seed the connection state (and LED) from PipeWire reality once both nodes are present.
    // GetConnectionState returns false if either node is still absent, which is the correct
    // default — we will be called again when the second node appears.
    const bool bConnected = m_audioService->GetConnectionState(m_Left, m_Right);
    if (bConnected != m_bIsConnected) {
        m_bIsConnected = bConnected;
        Refresh_LED_State();
    }
}

void Action_Connection::on_audio_system_ready() {
    m_bIsConnected = m_audioService->GetConnectionState(m_Left, m_Right);
    if (m_audioService->DoesNodeExist(m_Right) && m_audioService->DoesNodeExist(m_Left)) {
        if (m_bIsConnected != m_bDefaultState) {
            m_audioService->SetConnectionState(m_Left, m_Right, m_bDefaultState);
            m_bIsConnected = m_bDefaultState;
        }
    }
    Refresh_LED_State();
}

void Action_Connection::Init_Internal(const std::unordered_map<std::string, std::string> &params) {
    if (const auto It = params.find("left"); It != params.end()) {
        m_Left = It->second;
    } else {
        std::cerr << "Warning: Connection action is missing the \"left\" parameter – action will be inactive.\n";
    }
    if (const auto It = params.find("right"); It != params.end()) {
        m_Right = It->second;
    } else {
        std::cerr << "Warning: Connection action is missing the \"right\" parameter – action will be inactive.\n";
    }

    if (!m_Left.empty() && !m_Right.empty()) {
        if (const auto It = params.find("default-state"); It != params.end()) {
            m_audioService->RegisterNodeListener(m_Left, this);
            m_audioService->RegisterNodeListener(m_Right, this);
            m_audioService->RegisterSystemActivationListener(this);
            m_bDefaultState = It->second == "connected";
        } else {
            std::cerr << "Warning: Connection action between \"" << m_Left << "\" and \"" << m_Right
                      << "\" is missing \"default-state\" parameter – default state enforcement disabled.\n";
        }
        m_audioService->RegisterLinkListener(m_Left, m_Right, this);
    }
}
