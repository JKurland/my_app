#pragma once
#include <tuple>
#include <utility>
#include <type_traits>


template<typename T>
using remove_cvref_t = std::remove_cv_t<std::remove_reference_t<T>>;

template<typename T>
struct remove_cv_keep_ref {
    using type = T;
};

template<typename T>
struct remove_cv_keep_ref<T&> {
    using type = T&;
};

template<typename T>
struct remove_cv_keep_ref<const T&> {
    using type = T&;
};

template<typename T>
struct remove_cv_keep_ref<volatile T&> {
    using type = T&;
};

template<typename T>
struct remove_cv_keep_ref<volatile const T&> {
    using type = T&;
};

template<typename T>
struct remove_cv_keep_ref<T&&> {
    using type = T&&;
};

template<typename T>
struct remove_cv_keep_ref<const T&&> {
    using type = T&&;
};

template<typename T>
struct remove_cv_keep_ref<volatile T&&> {
    using type = T&&;
};

template<typename T>
struct remove_cv_keep_ref<volatile const T&&> {
    using type = T&&;
};

template<typename T>
using remove_cv_keep_ref_t = typename remove_cv_keep_ref<T>::type;

namespace detail {
    template<typename T>
    struct castable_to {
        // Used to test whether a callable is suitable to be dispatched to.
        // castable_to can be casted to anything T can be casted to but ignoring references.
        template<
            typename U,
            typename = std::enable_if_t<std::is_convertible_v<remove_cvref_t<T>, U> || std::is_convertible_v<remove_cvref_t<T>&, U>>,
            typename = void // default template param to remove ambiguity
        >
        operator U&() const;

        template<
            typename U,
            typename = std::enable_if_t<std::is_convertible_v<remove_cvref_t<T>, U> || std::is_convertible_v<remove_cvref_t<T>&, U>>
        >
        operator U&&() const;
    };

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
    constexpr std::false_type is_optional_impl(const T&) {return {};}

    template<typename T>
    constexpr std::true_type is_optional_impl(const std::optional<T>&) {return {};} 
}

template<typename...Ts, typename F, size_t...Is>
constexpr void static_for_each_index(std::tuple<Ts...>& t, F func, std::index_sequence<Is...>) {
    (void) func;
    (func(std::get<Is>(t)),...);
}

template<typename...Ts, typename F>
constexpr void static_for_each(std::tuple<Ts...>& t, F func) {
    static_for_each_index(t, func, std::make_index_sequence<sizeof...(Ts)>{});
}

template<typename F, typename...ArgTs>
struct can_call {
    static constexpr bool value = decltype(detail::can_call_impl<F, ArgTs...>(0))::value;
};

// It's much easier to work with the dispatch system if dispatch only depends on the raw type of the argument instead
// of how it is cv_ref qualified.
template<typename F, typename CtxT, typename ArgT>
struct dispatch_match {
    static constexpr bool value() {
        return can_call<F, remove_cv_keep_ref_t<CtxT>, detail::castable_to<ArgT>>::value || can_call<F, remove_cv_keep_ref_t<CtxT>, ArgT>::value;
    }
};

template<typename F, typename CtxT, typename ArgT>
constexpr bool dispatch_match_v = dispatch_match<F, CtxT, ArgT>::value();


namespace detail {
    template<typename First, typename...Rest, typename...ArgTs>
    constexpr auto count_dispatch_match_impl(const std::tuple<ArgTs...>& args) {
        if constexpr (dispatch_match_v<First, ArgTs...>) {
            if constexpr (sizeof...(Rest) > 0) {
                return detail::inc(count_dispatch_match_impl<Rest...>(args));
            } else {
                (void)args;
                return detail::Count<1>{};
            }
        } else {
            if constexpr (sizeof...(Rest) > 0) {
                return count_dispatch_match_impl<Rest...>(args);
            } else {
                (void)args;
                return detail::Count<0>{};
            }
        }
    }
}

template<typename ArgsTupleT, typename...HandlerTs>
constexpr int count_dispatch_match() {
    if constexpr (sizeof...(HandlerTs) == 0) {
        return 0;
    } else {
        return decltype(detail::count_dispatch_match_impl<HandlerTs...>(std::declval<ArgsTupleT>()))::value;
    }
}

template<typename ArgsTupleT, typename...HandlerTs>
constexpr bool any_dispatch_match() {
    return count_dispatch_match<ArgsTupleT, HandlerTs...>() > 0;
}

template<typename...Ts>
constexpr bool false_v = false;

struct NoHandlerError {
    template<typename EventT>
    NoHandlerError(EventT&&) {
        static_assert(false_v<EventT>, "No handler found for event of type EventT");
    }
};

struct NoRequestHandlerError {
    template<typename RequestT>
    NoRequestHandlerError(RequestT&&) {
        static_assert(false_v<RequestT>, "No handler found for request of type RequestT");
    }
};

// template<typename T>
// void print_type(T t) {
//     static_assert(!std::is_same_v<T, T>, "T");
// }

template<std::size_t I, std::size_t...Is>
constexpr auto seq_prepend(std::index_sequence<Is...>) {
    return std::index_sequence<I, Is...>{};
}

template<std::size_t I, std::size_t...Is>
constexpr auto seq_append(std::index_sequence<Is...>) {
    return std::index_sequence<Is..., I>{};
}

template<std::size_t...Is>
constexpr std::size_t last_idx(std::index_sequence<Is...>) {
    return std::remove_reference_t<decltype(std::get<sizeof...(Is)-1>(std::declval<std::tuple<detail::Count<Is>...>>()))>::value;
}

template<std::size_t First, std::size_t...Is>
constexpr auto head_indices(std::index_sequence<First, Is...>) {
    if constexpr(sizeof...(Is) == 0) {
        return std::index_sequence<>{};
    } else {
        if constexpr(sizeof...(Is) == 1) {
            return std::index_sequence<First>{};
        } else {
            return seq_prepend<First>(head_indices(std::index_sequence<Is...>{}));
        }
    }
}

namespace detail {
    template<size_t Idx, typename SeqT, typename ArgsTupleT, typename FirstHandlerT, typename...RestHandlerTs>
    constexpr auto dispatch_match_indices_impl() {
        if constexpr(any_dispatch_match<ArgsTupleT, FirstHandlerT>()) {
            using NewSeqT = decltype(seq_append<Idx>(std::declval<SeqT>()));
            if constexpr(sizeof...(RestHandlerTs) > 0) {
                return dispatch_match_indices_impl<Idx+1, NewSeqT, ArgsTupleT, RestHandlerTs...>();
            } else {
                return NewSeqT{};
            }
        } else {
            if constexpr(sizeof...(RestHandlerTs) > 0) {
                return dispatch_match_indices_impl<Idx+1, SeqT, ArgsTupleT, RestHandlerTs...>();
            } else {
                return SeqT{};
            }
        }
    }
}

template<typename ArgsTupleT, typename...HandlerTs>
constexpr auto dispatch_match_indices() {
    return decltype(detail::dispatch_match_indices_impl<0, std::index_sequence<>, ArgsTupleT, HandlerTs...>()){};
}

template<typename ArgsTupleT, typename...HandlerTs>
constexpr auto dispatch_match_head() {
    return decltype(head_indices(dispatch_match_indices<ArgsTupleT, HandlerTs...>())){};
}

template<typename ArgsTupleT, typename...HandlerTs>
constexpr auto dispatch_match_last() {
    return last_idx(dispatch_match_indices<ArgsTupleT, HandlerTs...>());
}
