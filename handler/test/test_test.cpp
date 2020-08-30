#include <string>
#include <optional>
#include <type_traits>

#include "gtest/gtest.h"
#include "event/serial.h"
#include "event/first.h"
#include "event/must_handle.h"
#include "event/meta.h"


struct MoveOnly {
    MoveOnly(std::string s): s(s) {}

    MoveOnly(MoveOnly&&) = default;
    MoveOnly& operator=(MoveOnly&&) = default;

    MoveOnly(const MoveOnly&) = delete;
    MoveOnly& operator=(const MoveOnly&) = delete;

    std::string s;
};

struct CopyOnly {
    CopyOnly(std::string s): s(s) {}
    CopyOnly(CopyOnly&&) = delete;
    CopyOnly& operator=(CopyOnly&&) = delete;
    CopyOnly(const CopyOnly&) = default;
    CopyOnly& operator=(const CopyOnly&) = default;

    std::string s;
};

struct CopyAndMove {
    CopyAndMove(std::string s): s(s) {}
    CopyAndMove(CopyAndMove&&) = default;
    CopyAndMove& operator=(CopyAndMove&&) = default;
    CopyAndMove(const CopyAndMove&) = default;
    CopyAndMove& operator=(const CopyAndMove&) = default;

    std::string s;
};

struct NoCopyNoMove {
    NoCopyNoMove(std::string s): s(s) {}
    NoCopyNoMove(NoCopyNoMove&&) = delete;
    NoCopyNoMove& operator=(NoCopyNoMove&&) = delete;
    NoCopyNoMove(const NoCopyNoMove&) = delete;
    NoCopyNoMove& operator=(const NoCopyNoMove&) = delete;

    std::string s;
};


TEST(TestSerial, one_handler) {
    int i = 0;
    auto handler = Serial {
        [](int& ctx, int event){ctx += event;},
    };

    handler(i, 5);
    ASSERT_EQ(i, 5);
}

TEST(TestSerial, two_handlers) {
    int i = 0;
    auto handler = Serial {
        [](int& ctx, int event){ctx += event;},
        [](int& ctx, std::string event){ctx += event.size();},
    };

    handler(i, 5);
    handler(i, "hello!");
    ASSERT_EQ(i, 11);
}

TEST(TestSerial, two_handlers_one_event) {
    int i = 0;
    auto handler = Serial {
        [](int& ctx, int event){ctx += event;},
        [](int& ctx, int event){ctx *= event;},
    };

    handler(i, 5);
    ASSERT_EQ(i, 25);
}

TEST(TestSerial, move_only) {
    int i = 0;
    auto handler = Serial {
        [](int& ctx, MoveOnly event){ctx += event.s.size();},
    };
    handler(i, MoveOnly{"hello"});
    ASSERT_EQ(i, 5);
}

TEST(TestSerial, move_only_nested) {
    int i = 0;
    auto handler = Serial {
        Serial {
            [](int& ctx, MoveOnly event){ctx += event.s.size();},
        }
    };
    handler(i, MoveOnly{"hello"});
    ASSERT_EQ(i, 5);
}

TEST(TestSerial, move_only_by_reference) {
    int i = 0;
    auto handler = Serial {
        [](int& ctx, const MoveOnly& event){ctx += event.s.size();},
        [](int& ctx, const MoveOnly& event){ctx *= event.s.size();},
    };
    handler(i, MoveOnly{"hello"});
    ASSERT_EQ(i, 25);
}

TEST(TestFirst, return_types) {
    int i = 0;
    auto void_handler = First {
        [](int& ctx, int x) {return;}
    };
    static_assert(std::is_void_v<decltype(void_handler(i, 3))>, "");

    auto optional_handler = First {
        [](int& ctx, int x) {return std::optional<int>(3);}
    };
    static_assert(std::is_same_v<decltype(optional_handler(i, 3)), std::optional<int>>, "");

    auto optional_handler2 = First {
        [](int& ctx, int x) {return;},
        [](int& ctx, int x) {return std::optional<int>(3);}
    };
    static_assert(std::is_same_v<decltype(optional_handler2(i, 3)), std::optional<int>>, "");

    auto definite_handler = First {
        [](int& ctx, int x) {return 3;}
    };
    static_assert(std::is_same_v<decltype(definite_handler(i, 3)), int>, "");

    auto definite_handler2 = First {
        [](int& ctx, int x) -> std::optional<int> {return {};},
        [](int& ctx, int x) {return 3;},
    };
    static_assert(std::is_same_v<decltype(definite_handler2(i, 3)), int>, "");

    auto definite_handler3 = First {
        [](int& ctx, int x) {return;},
        [](int& ctx, int x) -> std::optional<int> {return {};},
        [](int& ctx, int x) {return 3;},
    };
    static_assert(std::is_same_v<decltype(definite_handler3(i, 3)), int>, "");

    auto definite_handler4 = First {
        [](int& ctx, int x) {return;},
        [](int& ctx, int x) {return 3;},
        [](int& ctx, int x) -> std::optional<int> {return {};},
    };
    static_assert(std::is_same_v<decltype(definite_handler4(i, 3)), int>, "");
}

TEST(TestFirst, move_only) {
    int i = 0;
    auto handler = First {
        // the first handler is definitive so it can take MoveOnly by value
        // since we'll never have to call the second handler.
        [](int& ctx, MoveOnly request){return 2;},
        [](int& ctx, MoveOnly request){return 3;},
    };
    ASSERT_EQ(handler(i, MoveOnly{"hello"}), 2);
}

TEST(TestFirst, const_request) {
    int i = 0;
    const int request = 3;
    auto handler = First {
        [](int& ctx, const int& request){return request + 1;}
    };
    ASSERT_EQ(handler(i, request), 4);
}

TEST(TestMustHandle, move_only) {
    int i = 0;
    auto handler = 
    Serial {
        [](int& ctx, const MoveOnly& event){return;},
        MustHandle {
            Serial {
                [](int& ctx, const MoveOnly& event){return;},
                [](int& ctx, MoveOnly event){ctx += event.s.size();},
            },
        }
    };
    handler(i, MoveOnly{"hello"});
    ASSERT_EQ(i, 5);
}

struct CallableVal {
    template<typename T>
    void operator()(int& ctx, T t);
};

struct CallableRef {
    template<typename T>
    void operator()(int& ctx, T& t);
};

struct CallableConstRef {
    template<typename T>
    void operator()(int& ctx, const T& t);
};

struct CallableRefRef {
    template<typename T>
    void operator()(int& ctx, T&& t);
};

struct CallableConstRefRef {
    template<typename T>
    void operator()(int& ctx, const T&& t);
};

#define TEST_DISPATCH_MATCH(type) \
    static_assert(dispatch_match_v<CallableVal, int&, type>, ""); \
    static_assert(!dispatch_match_v<CallableRef, int&, type>, ""); \
    static_assert(dispatch_match_v<CallableConstRef, int&, type>, ""); \
    static_assert(dispatch_match_v<CallableRefRef, int&, type>, ""); \
    static_assert(dispatch_match_v<CallableConstRefRef, int&, type>, ""); \
\
    static_assert(dispatch_match_v<CallableVal, int&, type&>, ""); \
    static_assert(dispatch_match_v<CallableRef, int&, type&>, ""); \
    static_assert(dispatch_match_v<CallableConstRef, int&, type&>, ""); \
    static_assert(dispatch_match_v<CallableRefRef, int&, type&>, ""); \
    static_assert(dispatch_match_v<CallableConstRefRef, int&, type&>, ""); \
\
    static_assert(dispatch_match_v<CallableVal, int&, type&&>, ""); \
    static_assert(!dispatch_match_v<CallableRef, int&, type&&>, ""); \
    static_assert(dispatch_match_v<CallableConstRef, int&, type&&>, ""); \
    static_assert(dispatch_match_v<CallableRefRef, int&, type&&>, ""); \
    static_assert(dispatch_match_v<CallableConstRefRef, int&, type&&>, ""); \
\
    static_assert(dispatch_match_v<CallableVal, int&, const type>, ""); \
    static_assert(dispatch_match_v<CallableRef, int&, const type>, ""); \
    static_assert(dispatch_match_v<CallableConstRef, int&, const type>, ""); \
    static_assert(dispatch_match_v<CallableRefRef, int&, const type>, ""); \
    static_assert(dispatch_match_v<CallableConstRefRef, int&, const type>, ""); \
\
    static_assert(dispatch_match_v<CallableVal, int&, const type&>, ""); \
    static_assert(dispatch_match_v<CallableRef, int&, const type&>, ""); \
    static_assert(dispatch_match_v<CallableConstRef, int&, const type&>, ""); \
    static_assert(dispatch_match_v<CallableRefRef, int&, const type&>, ""); \
    static_assert(dispatch_match_v<CallableConstRefRef, int&, const type&>, ""); \
\
    static_assert(dispatch_match_v<CallableVal, int&, const type&&>, ""); \
    static_assert(dispatch_match_v<CallableRef, int&, const type&&>, ""); \
    static_assert(dispatch_match_v<CallableConstRef, int&, const type&&>, ""); \
    static_assert(dispatch_match_v<CallableRefRef, int&, const type&&>, ""); \
    static_assert(dispatch_match_v<CallableConstRefRef, int&, const type&&>, "");


TEST(TestDispatch, dispatch_to_function_template) {
    TEST_DISPATCH_MATCH(MoveOnly)
    TEST_DISPATCH_MATCH(CopyOnly)
    TEST_DISPATCH_MATCH(CopyAndMove)
    TEST_DISPATCH_MATCH(NoCopyNoMove)
}

#undef TEST_DISPATCH_MATCH


#define TEST_DISPATCH_MATCH(F) \
    static_assert(dispatch_match_v<F, int&, std::string>, ""); \
    static_assert(dispatch_match_v<F, int&, std::string&>, ""); \
    static_assert(dispatch_match_v<F, int&, std::string&&>, ""); \
    static_assert(dispatch_match_v<F, int&, const std::string>, ""); \
    static_assert(dispatch_match_v<F, int&, const std::string&>, ""); \
    static_assert(dispatch_match_v<F, int&, const std::string&&>, "");

TEST(TestDispatch, dispatch_to_function) {
    struct FVal { void operator()(int& ctx, std::string s){} };
    struct FRef { void operator()(int& ctx, std::string& s){} };
    struct FRefRef { void operator()(int& ctx, std::string&& s){} };
    struct FConstVal { void operator()(int& ctx, const std::string s){} };
    struct FConstRef { void operator()(int& ctx, const std::string& s){} };
    struct FConstRefRef { void operator()(int& ctx, const std::string&& s){} };

    TEST_DISPATCH_MATCH(FVal);
    TEST_DISPATCH_MATCH(FRef);
    TEST_DISPATCH_MATCH(FRefRef);

    TEST_DISPATCH_MATCH(FConstVal);
    TEST_DISPATCH_MATCH(FConstRef);
    TEST_DISPATCH_MATCH(FConstRefRef);
}

#undef TEST_DISPATCH_MATCH


#define TEST_DISPATCH_MATCH(F) \
    static_assert(!dispatch_match_v<F, int&, std::string>, ""); \
    static_assert(!dispatch_match_v<F, int&, std::string&>, ""); \
    static_assert(!dispatch_match_v<F, int&, std::string&&>, ""); \
    static_assert(!dispatch_match_v<F, int&, const std::string>, ""); \
    static_assert(!dispatch_match_v<F, int&, const std::string&>, ""); \
    static_assert(!dispatch_match_v<F, int&, const std::string&&>, "");

TEST(TestDispatch, dispatch_to_function_negative) {
    struct FVal { void operator()(int& ctx, float s){} };
    struct FRef { void operator()(int& ctx, float& s){} };
    struct FRefRef { void operator()(int& ctx, float&& s){} };
    struct FConstVal { void operator()(int& ctx, const float s){} };
    struct FConstRef { void operator()(int& ctx, const float& s){} };
    struct FConstRefRef { void operator()(int& ctx, const float&& s){} };

    TEST_DISPATCH_MATCH(FVal);
    TEST_DISPATCH_MATCH(FRef);
    TEST_DISPATCH_MATCH(FRefRef);

    TEST_DISPATCH_MATCH(FConstVal);
    TEST_DISPATCH_MATCH(FConstRef);
    TEST_DISPATCH_MATCH(FConstRefRef);
}

#undef TEST_DISPATCH_MATCH

TEST(TestDispatch, const_ctx) {
    // F requires non const ctx but still matches dispatch when a const ctx is provided.
    struct F { void operator()(int& ctx, float s){} };
    static_assert(dispatch_match_v<F, const int&, float>, "");
}