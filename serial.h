#pragma once
#include <tuple>
#include <type_traits>
#include "meta.h"


template<typename ...HandlerTs>
class Serial {
public:
    Serial(HandlerTs...handlers): handlers(std::move(handlers)...) {}

    template<typename CtxT, typename EventT>
    std::enable_if_t<any_can_call<std::tuple<CtxT&, EventT&>, HandlerTs...>()> operator() (CtxT& ctx, const EventT& event) {
        static_for_each(handlers, [&](auto& handler){
            if constexpr (can_call<decltype(handler), CtxT&, EventT&>::value) {
                handler(ctx, event);
            }
        });
    }

private:
    std::tuple<HandlerTs...> handlers;
};