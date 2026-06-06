#include "AudioService.h"

#include <algorithm>
#include <memory>
#include <ranges>
#include <string>
#include <vector>
#include <wp/object-manager.h>
#include <wireplumber-0.5/wp/spa-pod.h>
#include <wireplumber-0.5/wp/link.h>
#include <wireplumber-0.5/wp/port.h>
#include <pipewire-0.3/pipewire/keys.h>

#include "Actions/IAction.h"
#include "DebugLog.h"

void AudioService::Init(WpCore* core) {
    CoreRef = core;

    // Track all PipeWire links so GetConnectionState can query them on demand
    if (!ObjectManager) {
        LinksObjectManager = std::unique_ptr<WpObjectManager, WpObjMgrDeleter> { wp_object_manager_new() };
        wp_object_manager_add_interest(LinksObjectManager.get(), WP_TYPE_LINK, nullptr);
        wp_object_manager_request_object_features(LinksObjectManager.get(), WP_TYPE_LINK, WP_PIPEWIRE_OBJECT_FEATURE_INFO);
        g_signal_connect(LinksObjectManager.get(), "object-added",   G_CALLBACK(AudioService::on_link_added),   this);
        g_signal_connect(LinksObjectManager.get(), "object-removed", G_CALLBACK(AudioService::on_link_removed), this);
        wp_core_install_object_manager(core, LinksObjectManager.get());
    }

    // Track pipewrie nodes
    if (!ObjectManager) {
        ObjectManager = std::unique_ptr<WpObjectManager, WpObjMgrDeleter> { wp_object_manager_new() };
        wp_object_manager_add_interest(ObjectManager.get(), WP_TYPE_NODE, nullptr);
        wp_object_manager_request_object_features(ObjectManager.get(), WP_TYPE_NODE, WP_PIPEWIRE_OBJECT_FEATURES_MINIMAL | static_cast<WpObjectFeatures>(WP_NODE_FEATURE_PORTS));
        g_signal_connect(ObjectManager.get(), "object-added",   G_CALLBACK(AudioService::on_node_added),   this);
        g_signal_connect(ObjectManager.get(), "object-removed", G_CALLBACK(AudioService::on_node_removed), this);
        wp_core_install_object_manager(core, ObjectManager.get());
    }

    MixerApi = std::unique_ptr<WpPlugin, WpPluginDeleter>{wp_plugin_find(core, "mixer-api")};
    if (MixerApi == nullptr) {
        wp_core_load_component(
        core,
        "libwireplumber-module-mixer-api",
        "module",
        nullptr,
        "mixer-api",
        nullptr,
        reinterpret_cast<GAsyncReadyCallback>(&AudioService::on_module_loaded),
        this
    );
    }
}

void AudioService::Reset() {
    MixerApi.reset();
    LinksObjectManager.reset();
    ObjectManager.reset();
    Nodes.clear();
    ActiveLinks.clear();
    CoreRef = nullptr;
}

bool AudioService::DoesNodeExist(const std::string &nodeName) const {
    return Nodes.contains(nodeName);
}

bool AudioService::GetMute(const std::string& nodeName) {
    const auto Itr = Nodes.find(nodeName);
    if (Itr == Nodes.end()) return false;

    g_autoptr(WpIterator) Iterator = wp_pipewire_object_enum_params_sync(WP_PIPEWIRE_OBJECT(Itr->second.get()),
                                                                            "Props",
                                                                           nullptr);
    if (!Iterator) {
        return false;
    }

    g_auto(GValue) Item = G_VALUE_INIT;
    while (wp_iterator_next(Iterator, &Item)) {
        auto *WpPod = static_cast<WpSpaPod *>(g_value_get_boxed(&Item));
        gboolean is_muted = FALSE;
        if (wp_spa_pod_get_object(WpPod, nullptr, "mute", "b", &is_muted, nullptr)) {
            return is_muted;
        }
        g_value_unset(&Item);
    }

    // By default, if we hit this it means we don't have any mute state so we assume it's not muted
    return false;
}

void AudioService::SetVolume(const std::string& nodeName, float volume) {
    const auto Itr = Nodes.find(nodeName);
    if (Itr == Nodes.end() || !MixerApi) return;

    volume = std::clamp(volume, 0.0f, 1.0f);
    const guint32 NodeID = wp_proxy_get_bound_id(WP_PROXY(Itr->second.get()));
    GVariant* VolVariant = g_variant_new_double(volume);
    gboolean Success = FALSE;
    g_signal_emit_by_name(MixerApi.get(), "set-volume", NodeID, VolVariant, &Success);
}

void AudioService::SetMute(const std::string& nodeName, const bool bMute) {
    const auto Itr = Nodes.find(nodeName);
    if (Itr == Nodes.end() || !MixerApi) return;

    const guint32 nodeId = wp_proxy_get_bound_id(WP_PROXY(Itr->second.get()));
    GVariant* MuteVariant = g_variant_new_parsed("{'mute': <%b>}", bMute ? TRUE : FALSE);
    gboolean Success = FALSE;
    g_signal_emit_by_name(MixerApi.get(), "set-volume", nodeId, MuteVariant, &Success);

}

bool AudioService::GetConnectionState(const std::string& leftNode, const std::string& rightNode) {
    const auto LeftItr = Nodes.find(leftNode);
    const auto RightItr = Nodes.find(rightNode);
    if (LeftItr == Nodes.end() || RightItr == Nodes.end()) return false;

    const guint32 leftId  = wp_proxy_get_bound_id(WP_PROXY(LeftItr->second.get()));
    const guint32 rightId = wp_proxy_get_bound_id(WP_PROXY(RightItr->second.get()));

    for (const auto& [id, LinkPtr] : ActiveLinks) {
        guint32 OutNode, OutPort, InNode, InPort;
        wp_link_get_linked_object_ids(LinkPtr.get(), &OutNode, &OutPort, &InNode, &InPort);
        if (OutNode == leftId && InNode == rightId) {
            return true;
        }
    }
    return false;
}

void AudioService::SetConnectionState(const std::string& leftNode, const std::string& rightNode, bool bConnect) {
    const auto LeftItr  = Nodes.find(leftNode);
    const auto RightItr = Nodes.find(rightNode);
    if (LeftItr == Nodes.end() || RightItr == Nodes.end() || !CoreRef) return;

    LOG_DEBUG("Setting connection state between %s and %s to %s\n", leftNode.c_str(), rightNode.c_str(), bConnect ? "connected" : "disconnected");

    if (bConnect) {
        std::vector<guint32> OutputPortIds;
        {
            g_autoptr(WpIterator) Iterator = wp_node_new_ports_iterator(LeftItr->second.get());
            g_auto(GValue) Value = G_VALUE_INIT;
            while (wp_iterator_next(Iterator, &Value)) {
                if (auto* Port = WP_PORT(g_value_get_object(&Value)); wp_port_get_direction(Port) == WP_DIRECTION_OUTPUT) {
                    OutputPortIds.push_back(wp_proxy_get_bound_id(WP_PROXY(Port)));
                }
                g_value_unset(&Value);
            }
        }

        std::vector<guint32> InputPortIds;
        {
            g_autoptr(WpIterator) Iterator = wp_node_new_ports_iterator(RightItr->second.get());
            g_auto(GValue) Value = G_VALUE_INIT;
            while (wp_iterator_next(Iterator, &Value)) {
                if (auto* Port = WP_PORT(g_value_get_object(&Value)); wp_port_get_direction(Port) == WP_DIRECTION_INPUT) {
                    InputPortIds.push_back(wp_proxy_get_bound_id(WP_PROXY(Port)));
                }
                g_value_unset(&Value);
            }
        }

        // Sort both lists by ID for deterministic channel matching
        std::ranges::sort(OutputPortIds);
        std::ranges::sort(InputPortIds);
        const guint32 LeftId  = wp_proxy_get_bound_id(WP_PROXY(LeftItr->second.get()));
        const guint32 RightId = wp_proxy_get_bound_id(WP_PROXY(RightItr->second.get()));

        const std::size_t Count = std::min(OutputPortIds.size(), InputPortIds.size());
        for (std::size_t i = 0; i < Count; ++i) {
            WpProperties *Props = wp_properties_new(
            PW_KEY_LINK_OUTPUT_NODE, std::to_string(LeftId).c_str(),
                PW_KEY_LINK_INPUT_NODE, std::to_string(RightId).c_str(),
                PW_KEY_LINK_OUTPUT_PORT, std::to_string(OutputPortIds[i]).c_str(),
                PW_KEY_LINK_INPUT_PORT, std::to_string(InputPortIds[i]).c_str(),
                PW_KEY_OBJECT_LINGER, "true",
                nullptr);

            WpLink* Link = wp_link_new_from_factory(CoreRef, "link-factory", Props);
            if (!Link) {
                g_printerr("Failed to create link %u -> %u\n", OutputPortIds[i], InputPortIds[i]);
                continue;
            }
            wp_object_activate(WP_OBJECT(Link), WP_PIPEWIRE_OBJECT_FEATURES_ALL, nullptr, [](GObject* source, GAsyncResult* res, gpointer user_data) {
                g_autoptr(GError) Error = nullptr;
                if (!wp_object_activate_finish(WP_OBJECT(source), res, &Error)) {
                    g_printerr("Link activation failed: %s\n", Error->message);
                    // Activation failed — drop the ref we hold since on_link_added won't fire
                    g_object_unref(source);
                    return;
                }
                LOG_DEBUG("Link activated: %u\n", wp_proxy_get_bound_id(WP_PROXY(source)));
                // Ownership transfers to ActiveLinks via on_link_added; release the factory ref
                g_object_unref(source);
            }, nullptr);
        }
    } else {
        const guint32 LeftId  = wp_proxy_get_bound_id(WP_PROXY(LeftItr->second.get()));
        const guint32 RightId = wp_proxy_get_bound_id(WP_PROXY(RightItr->second.get()));

        std::vector<WpLink*> ToDestroy;
        for (const auto& [id, LinkPtr] : ActiveLinks) {
            guint32 OutNode, OutPort, InNode, InPort;
            wp_link_get_linked_object_ids(LinkPtr.get(), &OutNode, &OutPort, &InNode, &InPort);
            if ((OutNode == LeftId && InNode == RightId) || (OutNode == RightId && InNode == LeftId)) {
                ToDestroy.push_back(LinkPtr.get());
                LOG_DEBUG("Marked link for destruction\n");
            }
        }

        for (WpLink* Link : ToDestroy) {
            wp_global_proxy_request_destroy(WP_GLOBAL_PROXY(Link));
            // ActiveLinks entry is erased (and unref'd) by on_link_removed when PipeWire confirms removal
        }
    }
}

void AudioService::UnregisterNodeListener(const IActionBase* listener) {
    for (auto &val: NodeListeners | std::views::values) {
        for (auto Itr = val.rbegin(); Itr != val.rbegin(); ++Itr) {
            if (*Itr == listener) {
                val.erase(Itr.base());
            }
        }
    }
}

void AudioService::RegisterNodeListener(const std::string& nodeName, IActionBase* listener) {
    if (!NodeListeners.contains(nodeName)) {
        NodeListeners.emplace(nodeName, std::vector<IActionBase*>());
    }

    for (auto* Node : NodeListeners[nodeName]) {
        if (Node == listener) {
            return;
        }
    }
    NodeListeners[nodeName].push_back(listener);
}

void AudioService::RegisterLinkListener(const std::string &leftNode, const std::string &rightNode, IActionBase *listener) {
    const std::string LinkName = leftNode + "->" + rightNode;
    if (!ConnectionListeners.contains(LinkName)) {
        ConnectionListeners.emplace(LinkName, std::vector<IActionBase*>());
    }

    for (auto* Node : ConnectionListeners[LinkName]) {
        if (Node == listener) {
            return;
        }
    }
    ConnectionListeners[LinkName].push_back(listener);
}

void AudioService::UnregisterLinkListener(const IActionBase* listener) {
    for (auto &val: ConnectionListeners | std::views::values) {
        for (auto Itr = val.rbegin(); Itr != val.rbegin(); ++Itr) {
            if (*Itr == listener) {
                val.erase(Itr.base());
            }
        }
    }
}

void AudioService::on_link_added(WpObjectManager*, WpObject *Object, gpointer UserData) {
    if (!UserData || !Object) {
        return;
    }

    auto* Service = static_cast<AudioService*>(UserData);
    WpLink* Link = WP_LINK(Object);
    const guint32 BoundId = wp_proxy_get_bound_id(WP_PROXY(Link));

    LOG_DEBUG("Link added: %u\n", BoundId);

    // Double-insert guard — object manager can fire object-added more than once in edge cases
    if (!Service->ActiveLinks.contains(BoundId)) {
        Service->ActiveLinks.emplace(BoundId, std::unique_ptr<WpLink, WpLinkDeleter>(WP_LINK(g_object_ref(Link))));
    }

    guint32 OutNodeId = 0, OutPortID = 0, InNodeID = 0, InNodePort = 0;
    wp_link_get_linked_object_ids(Link, &OutNodeId, &OutPortID, &InNodeID, &InNodePort);

    std::string LeftNodeName;
    std::string RightNodeName;

    for (const auto& [name, nodePtr] : Service->Nodes) {
        const guint32 id = wp_proxy_get_bound_id(WP_PROXY(nodePtr.get()));
        if (id == OutNodeId) LeftNodeName = name;
        if (id == InNodeID)  RightNodeName = name;
        if (!LeftNodeName.empty() && !RightNodeName.empty()) break;
    }

    if (!LeftNodeName.empty() && !RightNodeName.empty()) {
        const auto LeftItr = Service->ConnectionListeners.find(LeftNodeName + "->" + RightNodeName);
        if (LeftItr != Service->ConnectionListeners.end()) {
            for (IActionBase* Listener : LeftItr->second) {
                Listener->on_link_created(Link, LeftNodeName, RightNodeName);
            }
        }
    }
}

void AudioService::on_link_removed(WpObjectManager*, WpObject *Object, gpointer UserData) {
    if (!UserData || !Object) {
        return;
    }

    auto* Service = static_cast<AudioService*>(UserData);
    WpLink* Link = WP_LINK(Object);
    const guint32 BoundId = wp_proxy_get_bound_id(WP_PROXY(Link));

    LOG_DEBUG("Link Removed: %u\n", BoundId);

    // Erase from ActiveLinks first — unique_ptr destructor calls g_object_unref automatically
    Service->ActiveLinks.erase(BoundId);


    guint32 OutNodeId = 0, OutPortID = 0, InNodeID = 0, InNodePort = 0;
    wp_link_get_linked_object_ids(Link, &OutNodeId, &OutPortID, &InNodeID, &InNodePort);

    std::string LeftNodeName;
    std::string RightNodeName;

    for (const auto& [name, nodePtr] : Service->Nodes) {
        const guint32 id = wp_proxy_get_bound_id(WP_PROXY(nodePtr.get()));
        if (id == OutNodeId) LeftNodeName = name;
        if (id == InNodeID)  RightNodeName = name;
        if (!LeftNodeName.empty() && !RightNodeName.empty()) break;
    }

    if (!LeftNodeName.empty() && !RightNodeName.empty()) {
        const auto LeftItr = Service->ConnectionListeners.find(LeftNodeName + "->" + RightNodeName);
        if (LeftItr != Service->ConnectionListeners.end()) {
            for (IActionBase* Listener : LeftItr->second) {
                Listener->on_link_removed(Link, LeftNodeName, RightNodeName);
            }
        }
    }
}

void AudioService::on_node_added(WpObjectManager*, WpObject* Object, gpointer UserData) {
    if (!UserData || !Object) {
        return;
    }

    auto* Service = static_cast<AudioService*>(UserData);

    auto* Node = WP_NODE(Object);
    const char* Name = wp_pipewire_object_get_property(WP_PIPEWIRE_OBJECT(Node), "node.name");
    if (!Name) return;

    LOG_DEBUG("Node added: %s\n", Name);

    // We cache it so that when any systems need to do anything with it, we know where it lives
    if (const auto Itr = Service->Nodes.find(Name); Itr == Service->Nodes.end()) {
        Service->Nodes[Name] = std::unique_ptr<WpNode, WpNodeDeleter>(WP_NODE(g_object_ref(Node)));
        g_signal_connect(Object, "params-changed", G_CALLBACK(AudioService::on_node_params_changed), Service);
        g_signal_connect(Object, "ports-changed", G_CALLBACK(AudioService::on_node_ports_changed), Service);
    }

    if (const auto Itr = Service->NodeListeners.find(Name); Itr != Service->NodeListeners.end()) {
        for (IActionBase* Listener : Itr->second) {
            Listener->on_node_available(Node, Name);
        }
    }
}

void AudioService::on_node_removed(WpObjectManager *, WpObject *Object, gpointer UserData) {
    if (!UserData || !Object) {
        return;
    }

    const auto Service = static_cast<AudioService*>(UserData);
    auto* Node = WP_NODE(Object);
    const char* Name = wp_pipewire_object_get_property(WP_PIPEWIRE_OBJECT(Node), "node.name");
    if (!Name) return;

    LOG_DEBUG("Node removed: %s\n", Name);

    Service->Nodes.erase(Name); // We remove the cache


    const auto Itr = Service->NodeListeners.find(Name);
    if (Itr != Service->NodeListeners.end()) {
        for (IActionBase* Listener : Itr->second) {
            Listener->on_node_removed(Node, Name);
        }
    }
}

void AudioService::on_node_params_changed(WpPipewireObject* Node, const gchar* ParamName, gpointer UserData) {
    if (!UserData || !Node) return;
    if (g_strcmp0(ParamName, "Props") != 0) return;

    const auto Service = static_cast<AudioService*>(UserData);

    const char* Name = wp_pipewire_object_get_property(WP_PIPEWIRE_OBJECT(Node), "node.name");

    // Node params changed — actions listening to this node can be notified here
    const auto Itr = Service->NodeListeners.find(Name);
    if (Itr != Service->NodeListeners.end()) {
        for (IActionBase* Listener : Itr->second) {
            Listener->on_node_params_changed(Node, Name);
        }
    }
}

void AudioService::on_node_ports_changed(WpNode* Node, gpointer UserData) {
    if (!UserData || !Node) return;

    auto* const Service = static_cast<AudioService*>(UserData);

    const char* Name = wp_pipewire_object_get_property(WP_PIPEWIRE_OBJECT(Node), "node.name");
    const guint NumPorts = wp_node_get_n_ports(Node);
    // Check for any connection state manually done and if any of the ndoes are changed connect them
    LOG_DEBUG("Node ports changed: %s (%d ports)\n", Name, NumPorts);
    if (NumPorts == 0) return;

    // Connect nodes that are pending connection
    const auto Itr = Service->NodeListeners.find(Name);
    if (Itr != Service->NodeListeners.end()) {
        for (IActionBase* Listener : Itr->second) {
            Listener->on_node_ports_changed(Node, Name, NumPorts);
        }
    }
}

void AudioService::on_module_loaded(WpCore *core, GAsyncResult *res, gpointer user_data) {
    if (!user_data || !res || !core) {
        g_printerr("User data is null in module loaded callback.\n");
        return;
    }
    const auto Service = static_cast<AudioService*>(user_data);
    g_autoptr(GError) error = nullptr;

    if (!wp_core_load_component_finish(core, res, &error)) {
        g_printerr("Failed to load component: %s\n", error->message);
        return;
    }

    LOG_DEBUG("Successfully loaded: %s\n", "mixer-api");

    if (WpPlugin* plugin = wp_plugin_find(core, "mixer-api")) {
        LOG_DEBUG("Successfully found %s pointer!\n", "mixer-api");
        Service->MixerApi = std::unique_ptr<WpPlugin, WpPluginDeleter>{plugin};
    }
}