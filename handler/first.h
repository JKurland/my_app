#pragma once
#include <tuple>
#include <optional>
#include <type_traits>
#include "meta.h"

namespace detail {
    template<typename T>
    constexpr bool is_definitive() {
        if constexpr (std::is_void_v<T>) {
            return false;
        } else if constexpr (decltype(is_optional_impl(std::declval<T>()))::value) {
            return false;
        } else {
            return true;
        }
    }

    template<typename ResultT>
    struct FirstResultWrapper {
        template<typename F>
        auto& operator<<(F&& f) {
            // Anytime this is called F is a handler that is being shadowed, that is F will never be called.
            // There are a few options of what to check for here. Could allow anything and take the silent shadowing.
            // Could just check the anything being shadowed has a compatible return type or could disallow anything being shadowed.
            // Atm it's just checking that any handler being shadowed has a compatible return type.
            using RetT = decltype(f());
            static_assert(std::is_same_v<RetT, ResultT> || std::is_same_v<RetT, std::optional<ResultT>> || false_v<F>, "F must either return ResultT or std::optional<ResultT>");
            return *this;
        }
        ResultT&& get(){return std::move(r);}
        ResultT r;
    };

    template<>
    struct FirstResultWrapper<void> {
        template<typename F>
        auto operator<<(F&& f) {
            return make_first_result(std::forward<F>(f));
        }

        void get(){}
    };

    template<typename T>
    struct FirstResultWrapper<std::optional<T>> {
        template<typename F, typename = std::enable_if_t<!std::is_same_v<decltype(std::declval<F>()()), T>>>
        auto& operator<<(F&& f) {
            static_assert(std::is_same_v<decltype(f()), std::optional<T>> || false_v<F>, "F must either return T or std::optional<T>");

            // avoiding unneeded compiler errors
            if constexpr (!std::is_void_v<decltype(f())>) {
                if (!r) {
                    r = f();
                }
            }
            return *this;
        }

        template<typename F, typename = std::enable_if_t<std::is_same_v<decltype(std::declval<F>()()), T>>>
        auto operator<<(F&& f) {
            if (r) {
                return FirstResultWrapper<T>{*r};
            } else {
                return FirstResultWrapper<T>{f()};
            }
        }

        std::optional<T>&& get(){return std::move(r);}
        std::optional<T> r;
    };

    template<typename F>
    auto make_first_result(F&& f) {
        if constexpr (std::is_void_v<decltype(f())>) {
            f();
            return FirstResultWrapper<void>{};
        } else {
            return FirstResultWrapper<decltype(f())>{f()};
        }
    }

    // can't use a lambda because we only want the
    // call operator to be instantiated when needed
    // otherwise we can't deal with non copy constructable requests
    template<typename HandlerT, typename CtxT, typename RequestT>
    struct FirstHandlerClosure {
        FirstHandlerClosure(HandlerT& handler, CtxT& ctx, RequestT&& request): handler(handler), ctx(ctx), request(std::forward<RequestT>(request)) {}
        HandlerT& handler;
        CtxT& ctx;
        RequestT request;

        auto operator()() {
            if constexpr (is_definitive<decltype(handler(ctx, std::forward<RequestT>(request)))>()) {
                // If the handler returns something that means we won't have to call
                // any more handlers then it's ok to forward the request. This is essentially the last
                // handler.
                return handler(ctx, std::forward<RequestT>(request));
            } else {
                static_assert(can_call<HandlerT&, CtxT&, const RequestT&>::value,
                    "HandlerT is not the final possible handler so HandlerT must be able to accept a const RequestT&"
                );
                return handler(ctx, static_cast<const RequestT&>(request));
            }
        }
    };

    template<typename HandlerT, typename CtxT, typename RequestT>
    struct FirstLastHandlerClosure {
        FirstLastHandlerClosure(HandlerT& handler, CtxT& ctx, RequestT&& request): handler(handler), ctx(ctx), request(std::forward<RequestT>(request)) {}
        HandlerT& handler;
        CtxT& ctx;
        RequestT request;

        auto operator()() {
            return handler(ctx, std::forward<RequestT>(request));
        }
    };

    template<std::size_t LastI, typename HandlerTup, typename CtxT, typename RequestT, std::size_t...HeadIs>
    auto first_impl(HandlerTup& handlers, std::index_sequence<HeadIs...>, CtxT& ctx, RequestT&& request) {
        // Using << means order of evaluation is defined. I'm not sure this actually matters
        // though because in this case the evaluation of each FirstHandlerClosure has
        // no side effects, it's operator<< that can have side effects. The reasons for <<
        // is more that it seems mildly descriptive. If there are performance consequences of
        // the defined ordering a more permissive operator could be used.
        auto wrapper = (
            FirstResultWrapper<void>{} 
            << ... <<
            FirstHandlerClosure<std::tuple_element_t<HeadIs, HandlerTup>, CtxT, RequestT>{
                std::get<HeadIs>(handlers),
                ctx,
                std::forward<RequestT>(request)
            }
        );

        return (wrapper << FirstLastHandlerClosure<std::tuple_element_t<LastI, HandlerTup>, CtxT, RequestT>{
            std::get<LastI>(handlers),
            ctx,
            std::forward<RequestT>(request)
        }).get();
    }
}


template<typename ...HandlerTs>
class First {
public:
    First(HandlerTs...handlers): handlers(handlers...) {}

    // Search for a handler based on remove_cvref_t<RequestT> because basing dispatch on just the type of request
    // seems like the easiest thing to work with
    template<typename CtxT, typename RequestT, typename = std::enable_if_t<any_dispatch_match<std::tuple<CtxT&, RequestT>, HandlerTs...>()>>
    auto operator() (CtxT& ctx, RequestT&& request) {
        constexpr auto head = dispatch_match_head<std::tuple<CtxT&, RequestT>, HandlerTs...>();
        constexpr std::size_t last = dispatch_match_last<std::tuple<CtxT&, RequestT>, HandlerTs...>();
        return detail::first_impl<last>(handlers, head, ctx, std::forward<RequestT>(request));
    }

    template<typename CtxT>
    void operator() (CtxT&, NoRequestHandlerError) = delete;
private:
    std::tuple<HandlerTs...> handlers;
};
