#include "Actions/IAction.h"
#include "AudioService.h"

IActionBase::~IActionBase() {
    m_audioService->UnregisterNodeListener(this);
    m_audioService->UnregisterLinkListener(this);
    m_audioService->UnregisterSystemActivationListener(this);
}

void IActionBase::Init(const std::unordered_map<std::string, std::string> &params, LedManager &LedManager,
                       AudioService &audioService, const NanoKontrol2::Control cc) {
    m_ledManager = &LedManager;
    m_audioService = &audioService;
    if (params.contains("target")) {
        m_Node = params.at("target");
    }
    m_Control = cc;
    Init_Internal(params);
}