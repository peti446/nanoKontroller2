#pragma once
#include <cstdint>
#include <functional>
#include <memory>

#include "MidiTypes.h"
#include "NanoKontrol2.hpp"
#include "RtMidi.h"


class MidiManager {
public:
    using EventHandler = std::function<void(const MidiEvent&)>;

    MidiManager() = default;
    ~MidiManager() = default;

    MidiManager(const MidiManager&) = delete;
    MidiManager& operator=(const MidiManager&) = delete;

    bool open(const std::string& portHint);
    void close();
    void setEventHandler(EventHandler handler);
    [[nodiscard]] bool sendControlChange(std::uint8_t channel, std::uint8_t cc, std::uint8_t value) const;
    [[nodiscard]] bool isOpen() const noexcept;

private:
    static void rtCallback(double deltaTime, std::vector<unsigned char> *msg, void* thisPtr);
    void handleRawMessage(double deltaTime, const std::vector<unsigned char>& msg) const;

    std::unique_ptr<RtMidiIn> m_in;
    std::unique_ptr<RtMidiOut> m_out;
    EventHandler m_handler;
    bool m_open = false;
};
