#pragma once
#include <tuple>
#include <type_traits>
#include "meta.h"


template<typename ...HandlerTs>
class Serial {
public:
    Serial(HandlerTs...handlers): handlers(std::move(handlers)...) {}
    
    // If only one of the our handlers wants the event we can forward the event
    // otherwise the event needs to be shared between the handlers.
    template<typename CtxT, typename EventT>
    std::enable_if_t<count_can_call<std::tuple<CtxT&, EventT>, HandlerTs...>() == 1> operator() (CtxT& ctx, EventT&& event) {
        static_for_each(handlers, [&](auto& handler){
            if constexpr (can_call<decltype(handler), CtxT&, EventT>::value) {
                handler(ctx, std::forward<EventT>(event));
            }
        });
    }

    template<typename CtxT, typename EventT>
    std::enable_if_t<(count_can_call<std::tuple<CtxT&, EventT&>, HandlerTs...>() > 1)> operator() (CtxT& ctx, const EventT& event) {
        static_for_each(handlers, [&](auto& handler){
            // check can call with EventT& to help people catch missing const specifiers. They will get
            // a compiler error instead of their event handler not being called
            if constexpr (can_call<decltype(handler), CtxT&, EventT&>::value) {
                handler(ctx, event);
            }
        });
    }

    template<typename CtxT>
    void operator() (CtxT&, NoHandlerError) = delete;
private:
    std::tuple<HandlerTs...> handlers;

};