#pragma once

#include <type_traits>
#include <tuple>
#include "meta.h"

// Wraps a handler and ensures that handler handles every event that can happen
// in a program.
template<typename HandlerT>
class MustHandle {
public:
    MustHandle(HandlerT handler): handler(std::move(handler)) {}

    template<typename CtxT, typename EventT, typename = std::enable_if_t<can_call<HandlerT, CtxT&, EventT>::value>>
    auto operator()(CtxT& ctx, EventT&& event) {
        return handler(ctx, std::forward<EventT>(event));
    }

    // This doesn't allow sfinae to ignore a lack of handlers
    template<typename CtxT, typename EventT>
    std::enable_if_t<!can_call<HandlerT, CtxT&, EventT>::value> operator()(CtxT& ctx, EventT&& event) {
        static_assert(!std::is_same_v<EventT, EventT>, "MustHandle could not find handler for EventT");
    }
private:
    HandlerT handler;
};
