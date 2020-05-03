#pragma once
#include <tuple>
#include <type_traits>
#include <memory>
#include "meta.h"

namespace detail {
    template<typename...ArgTs>
    struct SplitCanCall {
        // splits the handlers into all but the last
        // handler that can accept Args and the last handler
        // that can accept args.
        template<typename First, typename...Rest>
        auto operator()(First& first, Rest&...rest) {
            if constexpr (!can_call<First, ArgTs...>::value) {
                (void)first;
                if constexpr (sizeof...(rest) == 0) {
                    return nullptr;
                } else {
                    return (*this)(rest...);
                }
            } else {
                if constexpr (sizeof...(rest) == 0) {
                    return std::make_tuple(std::make_tuple(), std::addressof(first));
                } else {
                    // TODO force inline.
                    auto inner = (*this)(rest...);
                    if constexpr (std::is_null_pointer_v<decltype(inner)>) {
                        return std::make_tuple(std::make_tuple(), std::addressof(first));
                    } else {
                        return std::make_tuple(
                            std::tuple_cat(std::make_tuple(std::addressof(first)), std::get<0>(inner)),
                            std::get<1>(inner)
                        );
                    }
                }
            }
        }
    };

}

template<typename ...HandlerTs>
class Serial {
public:
    Serial(HandlerTs...handlers): handlers(std::move(handlers)...) {}
    

    template<typename CtxT, typename EventT>
    std::enable_if_t<(count_can_call<std::tuple<CtxT&, EventT>, HandlerTs...>() > 0)> operator() (CtxT& ctx, EventT&& event) {
        // check can call with EventT& to help people catch missing const specifiers. They will get
        // a compiler error instead of their event handler not being called
        auto [head, last] = std::apply(detail::SplitCanCall<CtxT&, EventT>{}, handlers);
        static_for_each(head, [&](auto& handler){
            (*handler)(ctx, static_cast<const EventT&>(event));
        });

        // forward the event to the last handler because event doesn't have to outlive
        // this function is we have ownership.
        (*last)(ctx, std::forward<EventT>(event));
    }

    template<typename CtxT>
    void operator() (CtxT&, NoHandlerError) = delete;
private:
    std::tuple<HandlerTs...> handlers;

};