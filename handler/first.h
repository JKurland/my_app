#pragma once
#include <tuple>
#include <optional>
#include <type_traits>
#include "meta.h"

namespace detail {
    template<typename T>
    constexpr bool is_definitive() {
        if constexpr (std::is_void_v<T>) {
            return true;
        } else if constexpr (decltype(is_optional_impl(std::declval<T>()))::value){
            return false;
        } else {
            return true;
        }
    }

    template<typename ResultT>
    struct FirstResultWrapper {
        template<typename F>
        auto& operator<<(F&&) {
            return *this;
        }
        ResultT&& get(){return std::move(r);}
        ResultT r;
    };

    template<>
    struct FirstResultWrapper<void> {
        template<typename F>
        auto operator<<(F&& f) {
            return make_first_result(f);
        }

        void get(){}
    };

    template<typename T>
    struct FirstResultWrapper<std::optional<T>> {
        template<typename F>
        auto& operator<<(F&& f) {
            if (!r) {
                if constexpr (std::is_void_v<decltype(f())>) {
                    f();
                } else {
                    r = f();
                }
            }
            return *this;
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
    // otherwise we can't deal with no copy constructable requests
    template<typename HandlerT, typename CtxT, typename RequestT>
    struct FirstHandlerClosure {
        HandlerT& handler;
        CtxT& ctx;
        RequestT& request;

        auto operator()() {
            if constexpr (is_definitive<decltype(handler(ctx, std::forward<RequestT>(request)))>()) {
                // If the handler returns something that means we won't have to call
                // any more handlers then it's ok to forward the request. This is essentially the last
                // handler.
                return handler(ctx, std::forward<RequestT>(request));
            } else {
                return handler(ctx, static_cast<const RequestT&>(request));
            }
        }
    };

    template<typename HandlerT, typename CtxT, typename RequestT>
    struct FirstLastHandlerClosure {
        HandlerT& handler;
        CtxT& ctx;
        RequestT& request;

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
                request
            }
        );

        return (wrapper << FirstLastHandlerClosure<std::tuple_element_t<LastI, HandlerTup>, CtxT, RequestT>{
            std::get<LastI>(handlers),
            ctx,
            request
        }).get();
    }
}


template<typename ...HandlerTs>
class First {
public:
    First(HandlerTs...handlers): handlers(handlers...) {}

    template<typename CtxT, typename RequestT, typename = std::enable_if_t<any_can_call<std::tuple<CtxT&, RequestT>, HandlerTs...>()>>
    auto operator() (CtxT& ctx, RequestT&& request) {
        constexpr auto head = can_call_head<std::tuple<CtxT&, RequestT>, HandlerTs...>();
        constexpr std::size_t last = can_call_last<std::tuple<CtxT&, RequestT>, HandlerTs...>();
        return detail::first_impl<last>(handlers, head, ctx, std::forward<RequestT>(request));
    }

    template<typename CtxT>
    void operator() (CtxT&, NoRequestHandlerError) = delete;
private:
    std::tuple<HandlerTs...> handlers;
};
