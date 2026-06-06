#include <wireplumber-0.5/wp/wp.h>
#include <glib.h>
#include <glib-unix.h>
#include <cxxopts.hpp>
#include <memory>
#include <unordered_map>
#include <concurrentqueue/moodycamel/concurrentqueue.h>

#include "AppSettings.h"
#include "AudioService.h"
#include "LedManager.h"
#include "MidiManager.h"
#include "NanoKontrol2.hpp"
#include "Actions/IAction.h"

struct GMainLoopDeleter  { void operator()(GMainLoop* ptr) const { g_main_loop_unref(ptr); } };
struct WpCoreDeleter     { void operator()(WpCore* ptr) const { g_clear_object(&ptr);   } };

constexpr int32_t MaxQueueSize = 10;
constexpr std::size_t TotalMemoryUsage = sizeof(MidiEvent) * 2 * MaxQueueSize;

struct AppState {
    AppState() : m_LedManager(m_MidiManager) {
    }
    std::unique_ptr<GMainLoop, GMainLoopDeleter> Loop = nullptr;
    std::unique_ptr<WpCore, WpCoreDeleter> Core = nullptr;



    MidiManager m_MidiManager;
    LedManager m_LedManager;
    AudioService m_AudioService;
    std::unordered_map<NanoKontrol2::Control, std::unique_ptr<IActionBase>> Actions;
    moodycamel::ConcurrentQueue<MidiEvent> QueuedMidiActions{MaxQueueSize};
};

static void on_midi_event(const MidiEvent& Event, AppState& State) {
    // NOTE: This is a separate thread, any changes to WP needs to happen on the main thread
    // We could send it directly via invoke, however to avoid many memory allocations we pre allocate a queue and simply do a producer/consumer pattern
    // this way we keep memory allocations at a minimum. If we supersede the max queue size, it will increase, and we will split into processing MaxQueueSize per ms
    State.QueuedMidiActions.enqueue(Event);
}

static gboolean on_signal(gpointer UserData) {
    auto& State = *static_cast<AppState*>(UserData);
    g_main_loop_quit(State.Loop.get());
    return G_SOURCE_REMOVE; // remove the source after firing
}

static void on_core_disconnected(WpCore* core, gpointer UserData) {
    auto& State = *static_cast<AppState*>(UserData);
    g_message("WirePlumber disconnected — attempting to reconnect...");

    State.m_AudioService.Reset();

    auto* retry = g_new(AppState*, 1);
    *retry = &State;
    g_timeout_add(500, [](gpointer data) -> gboolean {
        auto& S = **static_cast<AppState**>(data);
        if (wp_core_connect(S.Core.get())) {
            g_message("Reconnected to PipeWire/WirePlumber — re-initialising AudioService.");
            S.m_AudioService.Init(S.Core.get());
            g_free(data);
            return G_SOURCE_REMOVE; // stop retrying
        }
        g_message("Reconnect failed, retrying in 500 ms...");
        return G_SOURCE_CONTINUE;
    }, retry);
}

static void ConsumeQueue(AppState& State) {
    static std::vector<MidiEvent> Event;
    Event.resize(MaxQueueSize);
    std::size_t Dequeued = State.QueuedMidiActions.try_dequeue_bulk(Event.data(), MaxQueueSize);
    for (std::size_t i = 0; i < Dequeued; ++i) {
        auto EventData = Event.at(i);
        auto Itr = State.Actions.find(static_cast<NanoKontrol2::Control>(EventData.data1));
        if (Itr == State.Actions.end()) {
            return;
        }

        if (!Itr->second) {
            return;
        }

        Itr->second->on_midi(EventData.data2);
    }
}

static gboolean consume_queue_timeout(gpointer UserData) {
    auto& State = *static_cast<AppState*>(UserData);
    ConsumeQueue(State);
    return G_SOURCE_CONTINUE;
}


int main(const int argc, char** argv) {
    cxxopts::Options options("nanoKontroller2", "MIDI interface to control Pipewire nodes with the nanoKontrol2");
    options.add_options()
        ("h,help", "Print usage")
        ("c,config", "Path to config file", cxxopts::value<std::string>()->default_value("config.json"));

    const cxxopts::ParseResult SelectedOptions = options.parse(argc, argv);

    if (SelectedOptions.count("help"))
    {
        std::cout << options.help() << std::endl;
        return 0;
    }

    auto Settings = AppSettingsManager::GetAppSettings(SelectedOptions["config"].as<std::string>());

    wp_init(WP_INIT_PIPEWIRE);
    AppState State;
    State.Loop = std::unique_ptr<GMainLoop, GMainLoopDeleter>{ g_main_loop_new(nullptr, FALSE) };
    State.Core = std::unique_ptr<WpCore, WpCoreDeleter> { wp_core_new(nullptr, nullptr, nullptr) };

    if (!wp_core_connect(State.Core.get())) {
        std::cerr << "Failed to connect to PipeWire. Is the PipeWire daemon running?\n";
        return 1;
    }

    // Detect WirePlumber restarts: "disconnected" fires when the daemon goes away
    g_signal_connect(State.Core.get(), "disconnected", G_CALLBACK(on_core_disconnected), &State);

    // Before we hook to any events we set up all actions, midi controller and led controller
    if (!State.m_MidiManager.open("NanoKONTROL2")) {
        std::cerr << "Failed to open MIDI device \"NanoKONTROL2\". Make sure the device is connected.\n";
        return 1;
    }
    // We reset the state to disable all state from prev executions, this way our cache state is always up to date
    State.m_LedManager.ResetState();
    State.m_MidiManager.setEventHandler([&State]<typename T>(T&& Event) { on_midi_event(std::forward<T>(Event), State); });

    // We set up all actions here using the builder based on name and use magic_enum to conver the string into the right enim
    for (auto& action: Settings.Actions) {
        auto Enum = magic_enum::enum_cast<NanoKontrol2::Control>(action.first);
        if (!Enum.has_value()) {
            continue;
        }

        // We need to change all friendly names to the specific one, we are using target specifically here
        for (auto &val: action.second.params | std::views::values) {
            if (Settings.FriendlyNames.contains(val)) {
               val = Settings.FriendlyNames.at(val);
            }
        }
        State.Actions[*Enum] = std::move(ActionFactoryRegistry::BuildAction(action.second.type, action.second.params, State.m_LedManager, State.m_AudioService, Enum.value()));
        if (!State.Actions[*Enum]) {
            std::cerr << "Warning: Failed to build action of type \"" << action.second.type
                      << "\" for control \"" << action.first << "\" – action type may be unregistered.\n";
        }
    }
    State.m_AudioService.Init(State.Core.get());

    g_unix_signal_add(SIGINT,  on_signal, &State);
    g_unix_signal_add(SIGTERM, on_signal, &State);

    g_timeout_add(1, consume_queue_timeout, &State);
    g_main_loop_run(State.Loop.get());

    State.m_MidiManager.close();

    // Unique pointers are automatically cleaned up here
    return 0;
}