#pragma once
#include <memory>
#include <unordered_map>
#include <vector>
#include <wireplumber-0.5/wp/wp.h>

struct WpPluginDeleter { void operator()(WpPlugin* p) const          { g_object_unref(p);    } };
struct WpObjMgrDeleter { void operator()(WpObjectManager* ptr) const { g_clear_object(&ptr); } };
struct WpNodeDeleter   { void operator()(WpNode* p) const            { g_object_unref(p);    } };
struct WpLinkDeleter   { void operator()(WpLink* p) const            { g_object_unref(p);    } };

class IActionBase;

class AudioService {
public:
    void Init(WpCore* core);
    void Reset();

    bool IsSystemReady() const { return m_bLinksInstalled && m_bNodesInstalled; }
    bool DoesNodeExist(const std::string& nodeName) const;
    bool GetMute(const std::string& nodeName);
    void SetVolume(const std::string& nodeName, float volume);
    void SetMute(const std::string& nodeName, bool bMute);
    void SetConnectionState(const std::string& leftNode, const std::string& rightNode, bool bConnect);
    bool GetConnectionState(const std::string& leftNode, const std::string& rightNode);

    void UnregisterNodeListener(const IActionBase* listener);
    void UnregisterLinkListener(const IActionBase* listener);
    void UnregisterSystemActivationListener(const IActionBase* listener);
    void RegisterNodeListener(const std::string& nodeName, IActionBase* listener);
    void RegisterLinkListener(const std::string& leftNode, const std::string &rightNode, IActionBase* listener);
    void RegisterSystemActivationListener(IActionBase* listener);

private:
    // They are here instead of in the cpp so we can access private variables
    static void on_link_added(WpObjectManager*, WpObject* Object, gpointer UserData);
    static void on_link_removed(WpObjectManager*, WpObject* Object, gpointer UserData);
    static void on_node_added(WpObjectManager*, WpObject* Object, gpointer UserData);
    static void on_node_removed(WpObjectManager*, WpObject* Object, gpointer UserData);
    static void on_node_params_changed(WpPipewireObject* Node, const gchar* ParamName, gpointer UserData);
    static void on_module_loaded(WpCore *core, GAsyncResult *res, gpointer user_data);
    static void on_links_installed(WpObjectManager*, gpointer UserData);
    static void on_nodes_installed(WpObjectManager*, gpointer UserData);
    void on_all_install_completed() const;

    bool m_bNodesInstalled{false};
    bool m_bLinksInstalled{false};
    WpCore* CoreRef{nullptr};
    std::unique_ptr<WpObjectManager, WpObjMgrDeleter> ObjectManager{nullptr};
    std::unique_ptr<WpObjectManager, WpObjMgrDeleter> LinksObjectManager{nullptr};
    std::unique_ptr<WpPlugin, WpPluginDeleter> MixerApi{nullptr};
    std::unordered_map<std::string, std::unique_ptr<WpNode, WpNodeDeleter>> Nodes;
    std::unordered_map<guint32, std::unique_ptr<WpLink, WpLinkDeleter>> ActiveLinks;
    std::unordered_map<std::string, std::vector<IActionBase*>> NodeListeners;
    std::unordered_map<std::string, std::vector<IActionBase*>> ConnectionListeners;
    std::vector<IActionBase*> SystemActivationListeners;
};