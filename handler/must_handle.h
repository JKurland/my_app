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

    template<typename CtxT, typename T, typename = std::enable_if_t<can_call<HandlerT, CtxT&, T>::value>>
    auto operator()(CtxT& ctx, T&& event) {
        return handler(ctx, std::forward<T>(event));
    }

    // This doesn't allow sfinae to ignore a lack of handlers
    template<typename CtxT, typename T>
    std::enable_if_t<!can_call<HandlerT, CtxT&, T>::value> operator()(CtxT& ctx, T&& event) {
        static_assert(false_v<T>, "MustHandle could not find handler for T");
    }
private:
    HandlerT handler;
};
