#pragma once

#include <cstdint>
#include <magic_enum/magic_enum.hpp>

namespace NanoKontrol2 {

    // -------------------------------------------------------------------------
    // All controls: faders, knobs, and buttons
    // -------------------------------------------------------------------------
    enum class Control : uint8_t {
        // --- Channel strip faders (ch 1–8) ---
        Fader1 = 0,
        Fader2 = 1,
        Fader3 = 2,
        Fader4 = 3,
        Fader5 = 4,
        Fader6 = 5,
        Fader7 = 6,
        Fader8 = 7,

        // --- Channel strip knobs (ch 1–8) ---
        Knob1  = 16,
        Knob2  = 17,
        Knob3  = 18,
        Knob4  = 19,
        Knob5  = 20,
        Knob6  = 21,
        Knob7  = 22,
        Knob8  = 23,

        // --- Transport buttons (no LED) ---
        Rewind          = 43,
        FastForward     = 44,
        Stop            = 42,
        Play            = 41,
        Record          = 45,
        Cycle           = 46,

        PreviousTrack   = 58,
        NextTrack       = 59,
        Set             = 60,
        PreviousMarker  = 61,
        NextMarker      = 62,

        // --- Solo buttons (ch 1–8) – have LEDs ---
        Solo1 = 32,
        Solo2 = 33,
        Solo3 = 34,
        Solo4 = 35,
        Solo5 = 36,
        Solo6 = 37,
        Solo7 = 38,
        Solo8 = 39,

        // --- Mute buttons (ch 1–8) – have LEDs ---
        Mute1 = 48,
        Mute2 = 49,
        Mute3 = 50,
        Mute4 = 51,
        Mute5 = 52,
        Mute6 = 53,
        Mute7 = 54,
        Mute8 = 55,

        // --- Record (arm) buttons (ch 1–8) – have LEDs ---
        Rec1 = 64,
        Rec2 = 65,
        Rec3 = 66,
        Rec4 = 67,
        Rec5 = 68,
        Rec6 = 69,
        Rec7 = 70,
        Rec8 = 71,
    };

    // -------------------------------------------------------------------------
    // Only the controls that have a physical LED.
    // Send CC value 127 to turn ON, 0 to turn OFF (LED mode must be set to
    // "External" in the nanoKONTROL2 MIDI editor / Scene settings).
    // -------------------------------------------------------------------------
    enum class LED : uint8_t {
        // Transport
        Play        = static_cast<uint8_t>(Control::Play),
        Stop        = static_cast<uint8_t>(Control::Stop),
        Record      = static_cast<uint8_t>(Control::Record),
        Cycle       = static_cast<uint8_t>(Control::Cycle),

        // Solo (ch 1–8)
        Solo1 = static_cast<uint8_t>(Control::Solo1),
        Solo2 = static_cast<uint8_t>(Control::Solo2),
        Solo3 = static_cast<uint8_t>(Control::Solo3),
        Solo4 = static_cast<uint8_t>(Control::Solo4),
        Solo5 = static_cast<uint8_t>(Control::Solo5),
        Solo6 = static_cast<uint8_t>(Control::Solo6),
        Solo7 = static_cast<uint8_t>(Control::Solo7),
        Solo8 = static_cast<uint8_t>(Control::Solo8),

        // Mute (ch 1–8)
        Mute1 = static_cast<uint8_t>(Control::Mute1),
        Mute2 = static_cast<uint8_t>(Control::Mute2),
        Mute3 = static_cast<uint8_t>(Control::Mute3),
        Mute4 = static_cast<uint8_t>(Control::Mute4),
        Mute5 = static_cast<uint8_t>(Control::Mute5),
        Mute6 = static_cast<uint8_t>(Control::Mute6),
        Mute7 = static_cast<uint8_t>(Control::Mute7),
        Mute8 = static_cast<uint8_t>(Control::Mute8),

        // Record/Arm (ch 1–8)
        Rec1 = static_cast<uint8_t>(Control::Rec1),
        Rec2 = static_cast<uint8_t>(Control::Rec2),
        Rec3 = static_cast<uint8_t>(Control::Rec3),
        Rec4 = static_cast<uint8_t>(Control::Rec4),
        Rec5 = static_cast<uint8_t>(Control::Rec5),
        Rec6 = static_cast<uint8_t>(Control::Rec6),
        Rec7 = static_cast<uint8_t>(Control::Rec7),
        Rec8 = static_cast<uint8_t>(Control::Rec8),
    };

    constexpr auto AllLedValues = magic_enum::enum_values<LED>();

    constexpr uint8_t cc(Control c) noexcept {
        return static_cast<uint8_t>(c);
    }

    constexpr uint8_t cc(LED l) noexcept {
        return static_cast<uint8_t>(l);
    }

} // namespace NanoKontrol2

