#pragma once
#include <tuple>
#include <utility>
#include <type_traits>

namespace detail {
    template<typename...Ts, typename F, size_t...Is>
    constexpr void static_for_each(std::tuple<Ts...>& t, F func, std::index_sequence<Is...>) {
        (func(std::get<Is>(t)),...);
    }

    template<typename F, typename...ArgTs>
    constexpr std::true_type can_call_impl(decltype((std::declval<F>()(std::declval<ArgTs>()...), 0))) {return {};}

    template<typename...>
    constexpr std::false_type can_call_impl(...) {return {};}

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
    constexpr auto any_can_call_impl(std::tuple<ArgTs...> args) {
        if constexpr (can_call<First, ArgTs...>::value) {
            (void)args;
            return std::true_type{};
        } else {
            if constexpr (sizeof...(Rest) > 0) {
                return any_can_call_impl<Rest...>(args);
            } else {
                (void)args;
                return std::false_type{};
            }
        }
    }
}

template<typename ArgsTupleT, typename...HandlerTs>
constexpr bool any_can_call() {
    if constexpr (sizeof...(HandlerTs) == 0) {
        return false;
    } else {
        return decltype(detail::any_can_call_impl<HandlerTs...>(std::declval<ArgsTupleT>()))::value;
    }
}

