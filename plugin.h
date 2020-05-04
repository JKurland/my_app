#pragma once
// Just and example, won't actually compile and is missing a lot of details.

// An event the plugin is allowed to respond to
struct PluginVisibleEvent {
    int i;
};

// A request a plugin is allowed to make
struct PluginAllowedRequest {
    char c;
};

// Could probably do something clever like, to reduce duplication.
// using AllowedEvents = std::tuple<PluginVisibleEvent>;
// using AllowedRequests = std::tuple<PluginAllowedRequest>;

// Also possible to distinguish between events the pugin can handle
// and events it can emit, but this gets a bit tricky.

// This is an event handler plugin. You could also have a request handling plugin
// in the same way.
class Plugin {
public:
    // CtxT needs to always be the same type but Plugin<CtxT> would
    // be self referencial because Plugin is probably in CtxT somewhere.
    template<typename CtxT>
    Plugin(const PluginFiles& plugin) {
        interp.register_type<CtxT>("Ctx");
        interp.register_type<PluginVisibleEvent>("PluginVisibleEvent");
        interp.register_type<PluginAllowedRequest>("PluginAllowedRequest");
        
        interp.load(plugin);
    }

    template<typename CtxT>
    void operator()(CtxT& ctx, const PluginVisibleEvent& event) {
        // maybe could reregister Ctx for each event (cache for efficiency).
        // depends on the interpreter
        // interp.register_type<CtxT>("Ctx");
        interp.handle(ctx, event);
    }

private:
    // each plugin gets it's own interpreter, really nice for compatibility and having
    // some privileged plugins (made by us for example). Also might help with resilience
    // depending on how good the interpreter is.
    ChaiScriptInterpreter interp;
};
