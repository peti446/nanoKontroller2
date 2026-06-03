#include "LedManager.h"
#include "MidiManager.h"
#include <magic_enum/magic_enum.hpp>

LedManager::LedManager(MidiManager &midi) : m_midiManager(midi) {
}

void LedManager::ResetState() {
    for (const auto Led : NanoKontrol2::AllLedValues) {
        Set(Led, false);
    }
}

bool LedManager::Set(NanoKontrol2::LED control, const bool on, const std::uint8_t channel) {
    static_assert(std::is_same_v<magic_enum::underlying_type_t<NanoKontrol2::LED>, std::uint8_t>, "LED enum must be uint8_t");
    state_[control] = on;
    return m_midiManager.sendControlChange(channel, static_cast<std::uint8_t>(control), on ? 127 : 0);
}

bool LedManager::Toggle(const NanoKontrol2::LED control, const std::uint8_t channel) {
    // Default to off if we haven't seen this LED yet
    const auto it = state_.find(control);
    const bool current = (it != state_.end()) ? it->second : false;
    return Set(control, !current, channel);
}

bool LedManager::IsLedCapable(NanoKontrol2::Control control) noexcept {
    // LED enum values are a strict subset of Control values — check membership.
    return magic_enum::enum_contains<NanoKontrol2::LED>(static_cast<magic_enum::underlying_type_t<NanoKontrol2::Control>>(control));
}

std::optional<bool> LedManager::GetCachedState(const NanoKontrol2::LED control) const {
    if (const auto it = state_.find(control); it != state_.end()) {
        return it->second;
    }
    return std::nullopt;
}
