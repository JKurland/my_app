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
        auto split = std::apply(SplitCanCall<CtxT&, EventT>{}, handlers);
        auto head = std::get<0>(split);
        auto last = std::get<1>(split);
        static_for_each(head, [&](auto& handler){
            (*handler)(ctx, static_cast<const EventT&>(event));
        });

        // forward the event to the last handler because event doesn't have to outlive
        // this function if we have ownership. 
        (*last)(ctx, std::forward<EventT>(event));
    }

    // unlike MustHandle this allows sfinae to handle a lack of handlers
    template<typename CtxT>
    void operator() (CtxT&, NoHandlerError) = delete;
private:
    std::tuple<HandlerTs...> handlers;

};