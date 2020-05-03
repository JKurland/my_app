#pragma once

#include <type_traits>
#include <vector>
#include <memory>

#include "meta.h"

template<typename HandlerT>
class Dynamic {
public:
    Dynamic(std::vector<std::unique_ptr<HandlerT>> handlers): handlers(std::move(handlers)) {}

    template<typename CtxT, typename EventT, typename = std::enable_if_t<can_call<HandlerT, CtxT&, EventT>>>
    void operator()(CtxT& ctx, EventT&& event) {
        if (handlers.size() == 0) {
            return;
        }

        if (handlers.size() > 1) {
            for (auto handler = begin(handlers); handler != end(handlers) - 1; handler++) {
                (**handler)(ctx, static_cast<const EventT&>(event));
            }
        }

        (*handlers[handlers.size()-1])(ctx, std::forward<EventT>(event));
    }
private:
    std::vector<std::unqiue_ptr<HandlerT>> handlers;
};
