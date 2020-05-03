#pragma once
#include <tuple>
#include <utility>
#include <type_traits>

namespace detail {
    template<typename...Ts, typename F, size_t...Is>
    constexpr void static_for_each(std::tuple<Ts...>& t, F func, std::index_sequence<Is...>) {
        (void) func;
        (func(std::get<Is>(t)),...);
    }

    template<typename F, typename...ArgTs>
    constexpr std::true_type can_call_impl(decltype((std::declval<F&>()(std::declval<ArgTs>()...), 0))) {return {};}

    template<typename...>
    constexpr std::false_type can_call_impl(...) {return {};}

    template<int I>
    struct Count{
        static constexpr int value = I;
    };

    template<int I>
    Count<I+1> inc(Count<I>){return {};}

    template<typename T>
    constexpr std::false_type is_optional(const T&) {return {};}

    template<typename T>
    constexpr std::true_type is_optional(const std::optional<T>&) {return {};} 
}

template<typename...Ts, typename F>
constexpr void static_for_each(std::tuple<Ts...>& t, F func) {
    detail::static_for_each(t, func, std::make_index_sequence<sizeof...(Ts)>{});
}

template<typename F, typename...ArgTs>
struct can_call {
    static constexpr bool value = decltype(detail::can_call_impl<F, ArgTs...>(0))::value;
};

namespace detail {
    template<typename First, typename...Rest, typename...ArgTs>
    constexpr auto count_can_call_impl(const std::tuple<ArgTs...>& args) {
        if constexpr (can_call<First, ArgTs...>::value) {
            if constexpr (sizeof...(Rest) > 0) {
                return detail::inc(count_can_call_impl<Rest...>(args));
            } else {
                (void)args;
                return detail::Count<1>{};
            }
        } else {
            if constexpr (sizeof...(Rest) > 0) {
                return count_can_call_impl<Rest...>(args);
            } else {
                (void)args;
                return detail::Count<0>{};
            }
        }
    }
}

template<typename ArgsTupleT, typename...HandlerTs>
constexpr int count_can_call() {
    if constexpr (sizeof...(HandlerTs) == 0) {
        return 0;
    } else {
        return decltype(detail::count_can_call_impl<HandlerTs...>(std::declval<ArgsTupleT>()))::value;
    }
}

template<typename ArgsTupleT, typename...HandlerTs>
constexpr bool any_can_call() {
    return count_can_call<ArgsTupleT, HandlerTs...>() > 0;
}

struct NoHandlerError {
    template<typename EventT>
    NoHandlerError(EventT&&) {
        static_assert(!std::is_same_v<EventT, EventT>, "No handler found for event of type EventT");
    }
};

struct NoRequestHandlerError {
    template<typename RequestT>
    NoRequestHandlerError(RequestT&&) {
        static_assert(!std::is_same_v<RequestT, RequestT>, "No handler found for request of type RequestT");
    }
};

// template<typename T>
// void print_type(T t) {
//     static_assert(!std::is_same_v<T, T>, "T");
// }

template<typename...ArgTs>
struct SplitCanCall {
    // splits the handlers into all but the last
    // handler that can accept Args and the last handler
    // that can accept args.
    template<typename First, typename...Rest>
    constexpr auto operator()(First& first, Rest&...rest) const {
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
                const auto inner = (*this)(rest...);
                if constexpr (std::is_null_pointer_v<std::remove_const_t<decltype(inner)>>) {
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

template<typename T>
constexpr bool is_optional() {
    if constexpr (std::is_void_v<T>) {
        return false;
    } else {
        return decltype(detail::is_optional(std::declval<T>()))::value;
    }
}
