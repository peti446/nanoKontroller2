#include "MidiManager.h"

#include <glib.h>

namespace {
    bool checkContainsCaseInsensitive(std::string String, std::string ToFind) {
        auto toLower = [](unsigned char c){ return std::tolower(c); };
        std::transform(String.begin(), String.end(), String.begin(), toLower);
        std::transform(ToFind.begin(), ToFind.end(), ToFind.begin(), toLower);

        return String.find(ToFind) != std::string::npos;
    }
}

bool MidiManager::open(const std::string &portHint) {
    close();

    try {
        m_in = std::make_unique<RtMidiIn>();
        m_out = std::make_unique<RtMidiOut>();
    } catch (const RtMidiError &e) {
        std::cerr << "RtMidi create failed: " << e.getMessage() << '\n';
        m_in.reset();
        m_out.reset();
        return false;
    }


    const unsigned int inCount = m_in->getPortCount();
    const unsigned int outCount = m_out->getPortCount();

    std::optional<unsigned int> inPort;
    std::optional<unsigned int> outPort;

    for (unsigned int i = 0; i < inCount; ++i) {
        try {
            const std::string name = m_in->getPortName(i);
            if (::checkContainsCaseInsensitive(name, portHint)) {
                inPort = i;
                break;
            }
        } catch (const RtMidiError &e) {
            std::cerr << "RtMidiIn getPortName failed: " << e.getMessage() << '\n';
        }
    }

    for (unsigned int i = 0; i < outCount; ++i) {
        try {
            const std::string name = m_out->getPortName(i);
            if (::checkContainsCaseInsensitive(name, portHint)) {
                outPort = i;
                break;
            }
        } catch (const RtMidiError &e) {
            std::cerr << "RtMidiIn getPortName failed: " << e.getMessage() << '\n';
        }
    }

    if (!inPort || !outPort) {
        std::cerr << "Could not find MIDI ports matching hint: " << portHint << '\n';
        close();
        return false;
    }


    try {
        m_in->ignoreTypes(true, true, true);
        m_in->setCallback(&MidiManager::rtCallback, this);

        m_in->openPort(*inPort);
        m_out->openPort(*outPort);

        m_open = true;
        return true;
    } catch (const RtMidiError &e) {
        std::cerr << "RtMidi open failed: " << e.getMessage() << '\n';
        close();
        return false;
    }
}

void MidiManager::close() {
    m_open = false;

    if (m_in) {
        try {
            m_in->cancelCallback();
        } catch (...) {
        }
        if (m_in->isPortOpen()) {
            m_in->closePort();
        }
    }

    if (m_out && m_out->isPortOpen()) {
        m_out->closePort();
    }

    m_in.reset();
    m_out.reset();
}

void MidiManager::setEventHandler(EventHandler handler) {
    m_handler = std::move(handler);
}

bool MidiManager::sendControlChange(std::uint8_t channel, std::uint8_t cc, std::uint8_t value) const {
    if (!m_open || !m_out) {
        return false;
    }

    // MIDI channel is 0..15
    channel &= 0x0F;

    std::vector<unsigned char> msg{
        static_cast<unsigned char>(0xB0 | channel),
        static_cast<unsigned char>(cc),
        static_cast<unsigned char>(value)
    };

    try {
        m_out->sendMessage(&msg);
        return true;
    } catch (const RtMidiError &e) {
        std::cerr << "sendControlChange failed: " << e.getMessage() << '\n';
        return false;
    }
}

bool MidiManager::isOpen() const noexcept {
    return m_open;
}

void MidiManager::rtCallback(double deltaTime, std::vector<unsigned char> *msg, void *thisPtr) {
    if (!thisPtr || !msg) {
        return;
    }

    auto self = static_cast<MidiManager*>(thisPtr);
    self->handleRawMessage(deltaTime, *msg);
}

void MidiManager::handleRawMessage(double deltaTime, const std::vector<unsigned char> &msg) const {
    if (msg.empty()) {
        return;
    }

    std::uint8_t Data = msg[0];
    const auto Type = static_cast<std::uint8_t>(Data & 0xF0);
    const auto Channel = static_cast<std::uint8_t>(Data & 0x0F);

    MidiEvent ev;
    ev.channel = Channel;
    ev.data1 = msg[1];
    ev.data2 = msg[2];

    enum CommandType : std::uint8_t {
        ControlChange = 0xB0,
        NoteOn = 0x90,
        NoteOff = 0x80,
    };

    switch (Type) {
        case CommandType::ControlChange:
            ev.type = MidiMessageType::ControlChange;
            break;
        case CommandType::NoteOn:
            ev.type = (ev.data2 == 0) ? MidiMessageType::NoteOff : MidiMessageType::NoteOn;
            break;
        case CommandType::NoteOff:
            ev.type = MidiMessageType::NoteOff;
            break;
        default:
            ev.type = MidiMessageType::Unknown;
            break;
    }

    m_handler(ev);
}
