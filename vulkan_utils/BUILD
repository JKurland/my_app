cc_library(
    name = "vulkan_utils",
    hdrs = glob(["*.h"]),
    srcs = glob(["*.cpp"]),
    copts = ["-std=c++17"],
    include_prefix = "vulkan_utils",
    visibility = ["//visibility:public"],
    deps = ["@vulkan_hpp//:vulkan_hpp"],
)
