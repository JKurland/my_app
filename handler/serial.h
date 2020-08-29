#pragma once
#include <tuple>
#include <type_traits>
#include <memory>
#include "meta.h"


template<typename ...HandlerTs>
class Serial {
public:
    Serial(HandlerTs...handlers): handlers(std::move(handlers)...) {}

    template<typename CtxT, typename EventT, typename = std::enable_if_t<(count_can_call<std::tuple<CtxT&, EventT>, HandlerTs...>() > 0)>>
    void operator() (CtxT& ctx, EventT&& event) {
        // check can call with EventT to help people catch missing const specifiers. They will get
        // a compiler error instead of their event handler not being called

        constexpr auto head = can_call_head<std::tuple<CtxT&, EventT>, HandlerTs...>();
        static_for_each_index(handlers, [&](auto& handler){
            handler(ctx, static_cast<const EventT&>(event));
        }, head);

        // forward the event to the last handler, the last handler is
        // allowed to do whatever it wants to event (e.g. move out of it).
        constexpr std::size_t last = can_call_last<std::tuple<CtxT&, EventT>, HandlerTs...>();
        std::get<last>(handlers)(ctx, std::forward<EventT>(event));
    }

    // unlike MustHandle this allows sfinae to account for a lack of handlers
    // but will still display a compiler error message if called directly.
    template<typename CtxT>
    void operator() (CtxT&, NoHandlerError) = delete;
private:
    std::tuple<HandlerTs...> handlers;
};