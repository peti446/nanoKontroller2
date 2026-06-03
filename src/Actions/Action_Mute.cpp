#include "Actions/Action_Mute.h"

#include <bits/this_thread_sleep.h>
#include <iostream>

#include "AudioService.h"
#include "LedManager.h"

void Action_Mute::Init_Internal(const std::unordered_map<std::string, std::string> &params) {
    if (m_Node.empty()) {
        std::cerr << "Warning: Mute action has no \"target\" node specified – action will be inactive.\n";
        return;
    }
    m_audioService->RegisterNodeListener(m_Node, this);
    m_audioService->RegisterSystemActivationListener(this);
}

void Action_Mute::on_midi(int value) {
    if (value == 127) {
        if (m_bWaitingForLow) {
            // Still held down from a previous press, ignore
            return;
        }
        // First high: toggle mute and wait for the button to be released
        m_bWaitingForLow = true;
        m_bIsMuted = !m_bIsMuted;
        m_audioService->SetMute(m_Node, m_bIsMuted);
        if (LedManager::IsLedCapable(m_Control)) {
            m_ledManager->Set(static_cast<NanoKontrol2::LED>(m_Control), m_bIsMuted);
        }
    } else if (value == 0) {
        // Button released, ready to accept the next press
        m_bWaitingForLow = false;
    }
}

void Action_Mute::on_node_available(WpNode* node, const std::string &name) {
    if (name == m_Node) {
        const bool bOldValue = m_bIsMuted;
        m_bIsMuted = m_audioService->GetMute(m_Node);
        if (bOldValue != m_bIsMuted && LedManager::IsLedCapable(m_Control)) {
            m_ledManager->Set(static_cast<NanoKontrol2::LED>(m_Control), m_bIsMuted);
        }
    }
}

void Action_Mute::on_node_params_changed(WpPipewireObject * node, const std::string &name) {
    if (name == m_Node) {
        const bool bOldValue = m_bIsMuted;
        m_bIsMuted = m_audioService->GetMute(m_Node);
        if (bOldValue != m_bIsMuted && LedManager::IsLedCapable(m_Control)) {
            m_ledManager->Set(static_cast<NanoKontrol2::LED>(m_Control), m_bIsMuted);
        }
    }
}

void Action_Mute::on_audio_system_ready() {
    m_bIsMuted = m_audioService->GetMute(m_Node);
    if (LedManager::IsLedCapable(m_Control)) {
        m_ledManager->Set(static_cast<NanoKontrol2::LED>(m_Control), m_bIsMuted);
    }
}