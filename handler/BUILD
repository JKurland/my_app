cc_library(
    name = "event_lib",
    hdrs = glob(["*.h"]),
    copts = ["-std=c++17"],
    include_prefix = "event",
    visibility = ["//visibility:public"],
)

cc_test(
    name = "test_my_app",
    srcs = glob(["test/*.cpp"]),
    deps = ["@gtest//:gtest_main", ":event_lib"],
    linkopts = ["-lpthread"],
    copts = ["-std=c++17"],
)
