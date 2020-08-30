#pragma once

#include <type_traits>
#include <vector>

#include "meta.h"

template<typename HandlerT>
class Dynamic {
public:
    Dynamic(std::vector<HandlerT> handlers): handlers(std::move(handlers)) {}

    template<typename CtxT, typename EventT, typename = std::enable_if_t<dispatch_match_v<HandlerT, CtxT&, EventT>>>
    void operator()(CtxT& ctx, EventT&& event) {
        if (handlers.size() == 0) {
            return;
        }

        if (handlers.size() > 1) {
            for (auto handler = begin(handlers); handler != end(handlers) - 1; handler++) {
                (*handler)(ctx, static_cast<const EventT&>(event));
            }
        }

        handlers[handlers.size()-1](ctx, std::forward<EventT>(event));
    }
private:
    std::vector<HandlerT> handlers;
};
