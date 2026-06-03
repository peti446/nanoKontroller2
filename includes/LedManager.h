#pragma once
#include <cstdint>
#include <optional>

#include "NanoKontrol2.hpp"

class MidiManager;

class LedManager {
public:
    explicit LedManager(MidiManager& midi);
    void ResetState();
    bool Set(NanoKontrol2::LED control, bool on, std::uint8_t channel = 0);
    bool Toggle(NanoKontrol2::LED control, std::uint8_t channel = 0);

    [[nodiscard]] static bool IsLedCapable(NanoKontrol2::Control control) noexcept;
    [[nodiscard]] std::optional<bool> GetCachedState(NanoKontrol2::LED control) const;

private:
    //[[nodiscard]] std::optional<uint8_t> ToLedCc(NanoKontrol2::Control control) const noexcept;

    MidiManager& m_midiManager;
    std::unordered_map<NanoKontrol2::LED, bool> state_;
};