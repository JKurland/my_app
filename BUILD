load("@rules_foreign_cc//tools/build_defs:configure.bzl", "configure_make")

configure_make(
    name = "sdl2",
    lib_source = "@sdl2//:all",
    static_libraries = ["libSDL2.a", "libSDL2main.a"],
    shared_libraries = ["libSDL2-2.0.so.0.12.1"],
)

cc_binary(
    name = "my_app",
    srcs = glob(["main.cpp"]),
    copts = ["-std=c++17"],
    linkopts = ["-lpthread", "/usr/lib/x86_64-linux-gnu/libvulkan.so.1"],
    deps = ["//handler:event_lib", ":sdl2", "@vulkan_hpp//:vulkan_hpp"],
)
