#pragma once
#include <tuple>
#include <optional>
#include <type_traits>
#include "meta.h"


template<typename ...HandlerTs>
class First {
public:
    First(HandlerTs...handlers): handlers(handlers...) {}

    template<typename CtxT, typename RequestT, typename = std::enable_if_t<any_can_call<std::tuple<CtxT&, RequestT>, HandlerTs...>()>>
    auto operator() (CtxT& ctx, RequestT&& request) {
        auto split = std::apply(SplitCanCall<CtxT&, RequestT>{}, handlers);
        auto head = std::get<0>(split);
        auto last = std::get<1>(split);
        if constexpr (std::tuple_size_v<decltype(head)> > 0) {
            if constexpr (std::is_void_v<decltype(apply_first_impl(ctx, std::forward<RequestT>(request), head))>) {
                apply_first_impl(ctx, std::forward<RequestT>(request), head);
                return (*last)(ctx, std::forward<RequestT>(request));
            } else {
                auto ret = apply_first_impl(ctx, std::forward<RequestT>(request), head);

                if constexpr (!is_optional<decltype(ret)>()) {
                    return ret;
                } else {
                    if (ret) {
                        return ret;
                    }
                    if constexpr (std::is_void_v<decltype((*last)(ctx, std::forward<RequestT>(request)))>) {
                        (*last)(ctx, std::forward<RequestT>(request));
                        return ret;
                    } else {
                        return (*last)(ctx, std::forward<RequestT>(request));
                    }
                }
            }
        } else {
            return (*last)(ctx, std::forward<RequestT>(request));
        }
    }

    template<typename CtxT>
    void operator() (CtxT&, NoRequestHandlerError) = delete;
private:
    std::tuple<HandlerTs...> handlers;

    template<typename CtxT, typename RequestT, typename HandlerTup>
    auto apply_first_impl(CtxT& ctx, RequestT&& request, HandlerTup& handlers) {
        return std::apply([&](auto&...handler_pack){return first_impl(ctx, std::forward<RequestT>(request), handler_pack...);}, handlers);
    }

    template<typename CtxT, typename RequestT, typename FirstT, typename ...RestTs>
    auto first_impl(CtxT& ctx, RequestT&& request, FirstT& first, RestTs&...rest) {
        if constexpr (is_optional<decltype((*first)(ctx, std::forward<RequestT>(request)))>()) {
            auto ret = (*first)(ctx, static_cast<const RequestT&>(request));

            if constexpr (sizeof...(RestTs) > 0) {
                if (ret) {
                    return ret;
                }
                if constexpr (std::is_void_v<decltype(first_impl(ctx, std::forward<RequestT>(request), rest...))>) {
                    first_impl(ctx, std::forward<RequestT>(request), rest...);
                    return ret;
                } else {
                    return first_impl(ctx, std::forward<RequestT>(request), rest...);
                }
            } else {
                return ret;
            }
        } else {
            if constexpr (std::is_void_v<decltype((*first)(ctx, std::forward<RequestT>(request)))>) {
                (*first)(ctx, std::forward<RequestT>(request));
                if constexpr (sizeof...(RestTs) > 0) {
                    return first_impl(ctx, std::forward<RequestT>(request), rest...);
                } else {
                    return;
                }
            } else {
                return (*first)(ctx, std::forward<RequestT>(request));
            }
        }
    }
};
