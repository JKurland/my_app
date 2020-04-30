#pragma once
#include <tuple>
#include <optional>
#include <type_traits>
#include "meta.h"


template<typename ...HandlerTs>
class First {
public:
    First(HandlerTs...handlers): handlers(handlers...) {}

    template<typename CtxT, typename RequestT, typename = std::enable_if_t<any_can_call<std::tuple<CtxT&, RequestT&>, HandlerTs...>()>>
    auto operator() (CtxT& ctx, const RequestT& request) {
        return std::apply([&](auto...handler_pack){return first_impl_outer(ctx, request, handler_pack...);}, handlers);
    }
private:
    std::tuple<HandlerTs...> handlers;

    template<typename CtxT, typename RequestT, typename ...HandlerTs2>
    auto first_impl_outer(CtxT& ctx, const RequestT& request, HandlerTs2&...handlers) {
        return first_impl<std::optional<void>>(ctx, request, handlers...);
    }

    template<typename RetT, typename CtxT, typename RequestT, typename FirstT, typename ...RestTs>
    auto first_impl(CtxT& ctx, const RequestT& request, FirstT& first, RestTs&...rest) {
        if constexpr (can_call<decltype(first), CtxT&, RequestT&>::value) {
            auto ret = first(ctx, request);
            if (ret) {
                return ret;
            }

            if constexpr (sizeof...(RestTs) > 0) {
                return first_impl<decltype(ret)>(ctx, request, rest...);
            } else {
                return ret;
            }
        } else {
            if constexpr (sizeof...(RestTs) > 0) {
                return first_impl<RetT>(ctx, request, rest...);
            } else {
                return RetT{};
            }
        }
    }
};
