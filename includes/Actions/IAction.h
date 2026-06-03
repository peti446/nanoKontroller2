#pragma once

#include <functional>
#include <memory>
#include <wp/node.h>
#include <wp/proxy-interfaces.h>
#include <unordered_map>
#include <wp/link.h>

#include "ActionFactoryRegistry.h"
#include "NanoKontrol2.hpp"

class AudioService;
class LedManager;


class IActionBase {
public:
    IActionBase() = default;
    virtual ~IActionBase();

    void Init(const std::unordered_map<std::string, std::string> &params,
        LedManager& LedManager,
        AudioService& audioService,
        NanoKontrol2::Control cc);

    // Called when a MIDI CC message arrives for this action's key
    // value: 0-127
    virtual void on_midi(int value) = 0;

    // Called by WirePlumber when a node this action cares about appears.
    // The action should subscribe to params and sync LED state here.
    virtual void on_node_available(WpNode *node, const std::string &name) {}

    // Called when a node this action was using disappears.
    virtual void on_node_removed(WpNode* node, const std::string &name) {}

    // Called when a node params changed so internal state can be updated
    virtual void on_node_params_changed(WpPipewireObject * node, const std::string &name){}

    virtual void on_link_created(WpLink* link, const std::string& leftNodeName, const std::string& rightNodeName) {}
    virtual void on_link_removed(WpLink* link, const std::string& leftNodeName, const std::string& rightNodeName) {}
    virtual void on_audio_system_ready() {}

protected:
    virtual void Init_Internal(const std::unordered_map<std::string, std::string> &params) = 0;

    LedManager* m_ledManager{nullptr};
    AudioService* m_audioService{nullptr};
    NanoKontrol2::Control m_Control;
    // Maybe: Cache the Node* instead of name
    std::string m_Node;
};


template <std::size_t N>
struct FixedString {
    char value[N]{};

    constexpr FixedString(const char (&str)[N]) {
        for (std::size_t i = 0; i < N; ++i)
            value[i] = str[i];
    }

    [[nodiscard]] constexpr std::string_view view() const {
        return {value, N - 1};
    }
};

template <typename T, FixedString Name>
class IAction : public IActionBase
{
public:
    ~IAction() override = default;

private:
    friend T;
    struct Registrator {
        Registrator() {
            ActionFactoryRegistry::Register(std::string(Name.view()), [](const std::unordered_map<std::string, std::string>& params, LedManager& ledManager, AudioService& audioService, const NanoKontrol2::Control cc) -> std::unique_ptr<IActionBase> {
                std::unique_ptr<IActionBase> Ptr = std::make_unique<T>();
                Ptr->Init(params, ledManager, audioService, cc);
                return Ptr;
            });
        }
    };
    inline static Registrator _registrator;
    virtual void* touch() {
        return &_registrator;
    }

};