load("@bazel_tools//tools/build_defs/repo:git.bzl", "git_repository", "new_git_repository")
load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")

git_repository(
    name = "gtest",
    remote = "https://github.com/google/googletest.git",
    tag = "release-1.10.0",
)

# Change master to the git tag you want.
http_archive(
    name = "com_grail_bazel_toolchain",
    strip_prefix = "bazel-toolchain-master",
    urls = ["https://github.com/grailbio/bazel-toolchain/archive/master.tar.gz"],
)

load("@com_grail_bazel_toolchain//toolchain:deps.bzl", "bazel_toolchain_dependencies")

bazel_toolchain_dependencies()

load("@com_grail_bazel_toolchain//toolchain:rules.bzl", "llvm_toolchain")

llvm_toolchain(
    name = "llvm_toolchain",
    llvm_version = "10.0.0",
)

load("@llvm_toolchain//:toolchains.bzl", "llvm_register_toolchains")

llvm_register_toolchains()

http_archive(
   name = "rules_foreign_cc",
   strip_prefix = "rules_foreign_cc-master",
   url = "https://github.com/bazelbuild/rules_foreign_cc/archive/master.zip",
)
load("@rules_foreign_cc//:workspace_definitions.bzl", "rules_foreign_cc_dependencies")
rules_foreign_cc_dependencies([])

new_git_repository(
    name = "sdl2_source",
    remote = "https://github.com/spurious/SDL-mirror.git",
    commit = "2dfaaa3a821e029b2ddd48a84acea6a583c66638",
    build_file_content = """filegroup(name = "all", srcs = glob(["**"]), visibility = ["//visibility:public"])""",
)

new_git_repository(
    name = "vulkan_hpp",
    remote = "https://github.com/KhronosGroup/Vulkan-Headers.git",
    commit = "9250d5ae8f50202005233dc0512a1d460c8b4833",
    shallow_since = "1588589217 -0700",
    build_file_content = """
cc_library(
    name = "vulkan_hpp",
    hdrs = glob(["include/vulkan/*.hpp", "include/vulkan/*.h"]),
    visibility = ["//visibility:public"],
    strip_include_prefix = "include",
)
"""
)

git_repository(
    name = "glslang",
    remote = "https://github.com/KhronosGroup/glslang.git",
    commit = "bcf6a2430e99e8fc24f9f266e99316905e6d5134",
    shallow_since = "1587975125 -0600",
)

http_archive(
    name = "glm",
    url = "https://github.com/g-truc/glm/releases/download/0.9.9.8/glm-0.9.9.8.zip",
    strip_prefix = "glm",
    build_file_content = """
cc_library(
    name = "detail",
    hdrs = glob(["glm/detail/**/*.inl", "glm/detail/**/*.hpp"]),
    strip_include_prefix = "glm/detail",
    visibility = ["//visibility:public"],
)

cc_library(
    name = "glm",
    hdrs = glob(["glm/**/*.hpp", "glm/**/*.h", "glm/detail/**/*.inl"]),
    deps = [":detail"],
    visibility = ["//visibility:public"],
)
""",
    sha256 = "37e2a3d62ea3322e43593c34bae29f57e3e251ea89f4067506c94043769ade4c",
)
