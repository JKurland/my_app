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

    template<typename CtxT, typename T>
    auto operator()(CtxT& ctx, T&& event) {
        static_assert(dispatch_match_v<HandlerT, CtxT&, T>, "MustHandle could not find handler for T");
        return handler(ctx, std::forward<T>(event));
    }

private:
    HandlerT handler;
};
