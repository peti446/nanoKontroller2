# nanoKontroller2

A Linux daemon that bridges the **Korg nanoKONTROL2** MIDI controller with **PipeWire/WirePlumber**, letting you map physical faders, knobs, and buttons directly to audio node actions (muting, volume control, and toggling PipeWire link connections).
This is a rewrite in c++ of the original [nanoKontroller](https://github.com/tpneill/nanoKontroller) python script to have new features that I needed and to interact with wireplumberr directly instead of going through pulse.

## Features

- Maps every nanoKONTROL2 control (faders, knobs, Mute/Solo/Rec buttons, transport keys) to configurable actions.
- Integrates with PipeWire through WirePlumber – reacts to nodes appearing/disappearing in real time.
- LED feedback: button LEDs reflect the current state of the action (e.g. a Mute button lights up when the target is muted).
- Extensible action system – new action types can be added by implementing `IAction<T, "name">`.
- Lightweight producer/consumer MIDI queue keeps memory allocations minimal.

## Requirements

| Dependency | Notes |
|---|---|
| CMake ≥ 4.2 | Build system |
| C++26 compiler | GCC 14+ or Clang 18+ recommended |
| [WirePlumber](https://pipewire.pages.freedesktop.org/wireplumber/) (`wireplumber-0.5`) | PipeWire session manager library |
| GLib 2 / GObject 2 | Pulled in transitively with WirePlumber |
| [ALSA](https://alsa-project.org/) | Required by RtMidi on Linux |
| [RtMidi](https://www.music.mcgill.ca/~gary/rtmidi/) | MIDI I/O |
| [glaze](https://github.com/stephenberry/glaze) | JSON config parsing |
| [cxxopts](https://github.com/jarro2783/cxxopts) | Command-line argument parsing |
| [concurrentqueue](https://github.com/cameron314/concurrentqueue) | Lock-free MIDI event queue |
| [magic_enum](https://github.com/Neargye/magic_enum) | Enum ↔ string conversion for control names |

On a typical Arch / Fedora / Ubuntu system the system libraries can be installed with the package manager; the header-only/CMake libraries (glaze, cxxopts, concurrentqueue, magic_enum) are easiest to obtain via [vcpkg](https://vcpkg.io/) or [Conan](https://conan.io/).

## Building

```bash
# 1. Clone the repository
git clone https://github.com/your-username/nanoKontroller2.git
cd nanoKontroller2

# 2. Configure
cmake -B build -DCMAKE_BUILD_TYPE=Release

# 3. Compile
cmake --build build -j$(nproc)

# 4. (Optional) install
sudo cmake --install build
```

The resulting binary is `build/nanoKontroller2`.

## nanoKONTROL2 Device Setup

The nanoKONTROL2 must be configured so that its LEDs are driven externally (not by the device itself):

1. Open the **Korg KONTROL Editor** (available from Korg's website).
2. Set the **LED Mode** for every button you want feedback on to **"External"**.
3. Write the scene to the device.

Without this step the LED feedback will not work correctly.

## Usage

```
nanoKontroller2 [options]

Options:
  -h, --help          Print help and exit
  -c, --config PATH   Path to the JSON config file (default: config.json)
```

**Example:**

```bash
./nanoKontroller2 --config /etc/nanokontroller2/config.json
```

The daemon connects to PipeWire, opens the nanoKONTROL2 MIDI port, and starts processing events. Send `SIGINT` (Ctrl+C) or `SIGTERM` to stop it cleanly.

## Configuration File

The configuration is a JSON file. Copy `config-example.json` as a starting point:

```bash
cp config-example.json config.json
```

### Top-level structure

```jsonc
{
  // Optional: human-friendly aliases for PipeWire node names
  "FriendlyNames": {
    "alias": "actual.pipewire.node.name"
  },

  // Required: map nanoKONTROL2 control names to actions
  "actions": {
    "<ControlName>": {
      "type": "<action-type>",
      "params": {
        // action-specific parameters
      }
    }
  }
}
```

> **Note:** The JSON key for actions is `"actions"` (lowercase). The key for aliases is `"FriendlyNames"` (mixed case).

### Control names

Use the exact name from the table below as the key inside `"actions"`:

| Category | Names | Has LED |
|---|---|---|
| **Faders** | `Fader1` … `Fader8` | No |
| **Knobs** | `Knob1` … `Knob8` | No |
| **Mute buttons** | `Mute1` … `Mute8` | ✅ |
| **Solo buttons** | `Solo1` … `Solo8` | ✅ |
| **Rec/Arm buttons** | `Rec1` … `Rec8` | ✅ |
| **Transport** | `Play`, `Stop`, `Record`, `Cycle` | ✅ |
| **Transport** | `Rewind`, `FastForward` | No |
| **Marker / Track** | `PreviousTrack`, `NextTrack`, `Set`, `PreviousMarker`, `NextMarker` | No |

Controls marked ✅ support LED feedback – their LED state is automatically managed by the assigned action.

---

### Available action types

#### `Mute`

Toggles the mute state of a PipeWire node. If the assigned control has an LED, it lights up when the target node is muted and turns off when it is unmuted.

| Parameter | Required | Description |
|---|---|---|
| `target` | ✅ | PipeWire node name (or a `FriendlyNames` alias) to mute/unmute |

**Example – mute a sink with `Mute1`:**

```json
"Mute1": {
  "type": "Mute",
  "params": {
    "target": "music-audio-sink"
  }
}
```

---

#### `volume`

Maps a continuous control (fader or knob, range 0–127) to the volume of a PipeWire node. The MIDI value is converted to a linear volume using a power curve: `volume = (midi / 127) ^ pow`.

| Parameter | Required | Default | Description |
|---|---|---|---|
| `target` | ✅ | — | PipeWire node name (or a `FriendlyNames` alias) to control |
| `pow` | No | `4.0` | Exponent for the power curve. Higher values make the lower end of the fader more granular. Use `1.0` for a linear response. |

**Example – control a sink volume with `Fader1`:**

```json
"Fader1": {
  "type": "volume",
  "params": {
    "target": "music-audio-sink",
    "pow": "3.0"
  }
}
```

---

#### `connection`

Toggles a PipeWire link (connection) between two nodes. Pressing the assigned button connects or disconnects the two nodes. If the assigned control has an LED, it lights up when the link is active (connected).

When either node appears in PipeWire, the daemon automatically enforces `default-state` if the current link state differs from it.

| Parameter | Required | Description |
|---|---|---|
| `left` | ✅ | Source (output) PipeWire node name (or alias) |
| `right` | ✅ | Sink (input) PipeWire node name (or alias) |
| `default-state` | ✅ | Initial state to enforce when both nodes are present: `"connected"` or `"disconnected"` |

**Example – toggle a link between two nodes with `Solo1`:**

```json
"Solo1": {
  "type": "connection",
  "params": {
    "left":          "my-source-node",
    "right":         "my-sink-node",
    "default-state": "disconnected"
  }
}
```

---

### FriendlyNames

`FriendlyNames` lets you give a short alias to a long or unstable PipeWire node name. Wherever you use the alias as a `target`, `left`, or `right`, the daemon substitutes the real node name automatically.

```json
{
  "FriendlyNames": {
    "spotify": "Spotify - Music and Podcasts",
    "mic":     "alsa_input.usb-my_microphone.mono-fallback"
  }
}
```

> **Tip:** Run `wpctl status` or `pw-cli list-objects` to find the exact PipeWire node names on your system.

---

### Full example

The following config demonstrates all three action types:

```json
{
  "FriendlyNames": {
    "music": "music-output-sink",
    "mic":   "alsa_input.usb-my_microphone.mono-fallback",
    "loopback-src": "my-loopback-source",
    "loopback-dst": "my-loopback-sink"
  },
  "actions": {
    "Fader1": {
      "type": "volume",
      "params": {
        "target": "music",
        "pow": "4.0"
      }
    },
    "Fader2": {
      "type": "volume",
      "params": {
        "target": "mic",
        "pow": "1.0"
      }
    },
    "Mute1": {
      "type": "Mute",
      "params": { "target": "music" }
    },
    "Mute2": {
      "type": "Mute",
      "params": { "target": "mic" }
    },
    "Solo1": {
      "type": "connection",
      "params": {
        "left":          "loopback-src",
        "right":         "loopback-dst",
        "default-state": "disconnected"
      }
    }
  }
}
```

---

## Extending with new actions

1. Create a header in `includes/Actions/` and a source file in `src/Actions/`.
2. Derive from `IAction<YourClass, "your-type-name">`:

```cpp
// includes/Actions/Action_MyAction.h
#pragma once
#include "IAction.h"

class Action_MyAction final : public IAction<Action_MyAction, "my-action"> {
public:
    void on_midi(int value) override;
protected:
    void Init_Internal(const std::unordered_map<std::string, std::string>& params) override;
};
```

3. Implement the virtual methods. Optionally override `on_node_available`, `on_node_removed`, `on_node_params_changed`, `on_link_created`, `on_link_removed`, and `on_audio_system_ready` for event-driven behaviour.
4. The action is automatically registered with the factory via the `IAction` template – no further wiring is needed.
5. In the config use `"type": "my-action"`.

---

## Project structure

```
includes/
  AppSettings.h              – Config structs (AppSettings, ActionConfig)
  AudioService.h             – WirePlumber/PipeWire audio node interface
  LedManager.h               – Sends LED on/off messages to the device
  MidiManager.h              – RtMidi wrapper (open port, send/receive CC)
  MidiTypes.h                – MidiEvent struct
  NanoKontrol2.hpp           – Control & LED enum definitions
  Actions/
    IAction.h                – IActionBase + self-registering IAction<T, Name> template
    ActionFactoryRegistry.h  – Singleton factory: maps type strings → constructors
    Action_Mute.h            – "Mute" action declaration
    Action_Volume.h          – "volume" action declaration
    Action_Connection.h      – "connection" action declaration
src/
  main.cpp                   – Entry point, GLib main loop, wiring
  AppSettings.cpp            – JSON config loading via glaze
  AudioService.cpp           – WirePlumber integration (nodes, links, params)
  LedManager.cpp             – LED CC message dispatch
  MidiManager.cpp            – RtMidi port management and CC I/O
  Actions/
    IAction.cpp              – IActionBase destructor & Init()
    ActionFactoryRegistry.cpp – Factory registration storage
    Action_Mute.cpp          – Toggle mute on a PipeWire node
    Action_Volume.cpp        – Set volume on a PipeWire node (power curve)
    Action_Connection.cpp    – Toggle a PipeWire link between two nodes
CMakeLists.txt               – Build definition
config-example.json          – Starter configuration
```