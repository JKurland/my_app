cc_library(
    name = "event_lib",
    hdrs = glob(["*.h"]),
    copts = ["-std=c++17"],
    include_prefix = "event",
)

cc_binary(
    name = "my_app",
    srcs = glob(["main.cpp"]),
    copts = ["-std=c++17"],
    linkopts = ["-lpthread"],
    deps = [":event_lib"],
)

cc_test(
    name = "test_my_app",
    srcs = glob(["test/*.cpp"]),
    deps = ["@gtest//:gtest_main", ":event_lib"],
    linkopts = ["-lpthread"],
    copts = ["-std=c++17"],
)
