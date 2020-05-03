#include <string>

#include "gtest/gtest.h"
#include "event/serial.h"


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

struct MoveOnly {
    MoveOnly(std::string s): s(s) {}

    MoveOnly(MoveOnly&&) = default;
    MoveOnly& operator=(MoveOnly&&) = default;

    MoveOnly(const MoveOnly&) = delete;
    MoveOnly& operator=(const MoveOnly&) = delete;

    std::string s;
};

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


