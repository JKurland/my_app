load("@rules_foreign_cc//tools/build_defs:configure.bzl", "configure_make")
load("//glsl_rules:rules.bzl", "spirv")

configure_make(
    name = "sdl2",
    lib_source = "@sdl2_source//:all",
    static_libraries = ["libSDL2.a", "libSDL2main.a"],
)

cc_binary(
    name = "my_app",
    srcs = ["main.cpp"],
    copts = ["-std=c++17"],
    linkopts = ["-lpthread", "/usr/lib/x86_64-linux-gnu/libvulkan.so.1"],
    deps = [
        "//handler:event_lib",
        ":sdl2",
        "@vulkan_hpp//:vulkan_hpp",
        "//vulkan_utils:vulkan_utils",
        "@glm//:glm",
    ],
    data = [":test_spirv"],
)

spirv(
    name = "test_spirv",
    srcs = glob(["shaders/*"]),
    spirv_compiler = "@glslang//:glslangValidator",
)
