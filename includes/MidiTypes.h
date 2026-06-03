#pragma once
#include <cstdint>

enum class MidiMessageType : std::uint8_t { ControlChange, NoteOn, NoteOff, Unknown };

struct MidiEvent {
    MidiMessageType type = MidiMessageType::Unknown;
    std::uint8_t channel = 0;
    std::uint8_t data1 = 0;   // CC or note
    std::uint8_t data2 = 0;   // value/velocity
};
