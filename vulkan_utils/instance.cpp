#include "instance.h"

#include <vector>

#include "vulkan/vulkan.hpp"

namespace {
    std::vector<const char*> c_strs(const std::vector<std::string>& strs) {
        std::vector<const char*> rtn;
        for (const auto& str: strs) {
            rtn.push_back(str.c_str());
        }
        return rtn;
    }
}


vk::UniqueInstance InstanceBuilder::build() {
    // initialize the vk::ApplicationInfo structure
    vk::ApplicationInfo application_info(
        app_name_.c_str(),
        app_version_,
        engine_name_.c_str(),
        engine_version_,
        VK_API_VERSION_1_1
    );

    // initialize the vk::InstanceCreateInfo
    auto layer_c_strs = c_strs(layers_);
    auto extension_c_strs = c_strs(extensions_);
    vk::InstanceCreateInfo instance_create_info(
        flags_, &application_info,
        layer_c_strs.size(), layer_c_strs.data(),
        extension_c_strs.size(), extension_c_strs.data()
    );

    // create a UniqueInstance
    return vk::createInstanceUnique( instance_create_info );
}

InstanceBuilder& InstanceBuilder::extensions(std::vector<std::string> extensions) {
    extensions_ = std::move(extensions);
    return *this;
}

InstanceBuilder& InstanceBuilder::layers(std::vector<std::string> layers) {
    layers_ = std::move(layers);
    return *this;
}

InstanceBuilder& InstanceBuilder::app_name(std::string app_name) {
    app_name_ = std::move(app_name);
    return *this;
}

InstanceBuilder& InstanceBuilder::app_version(std::uint32_t app_version) {
    app_version_ = app_version;
    return *this;
}

InstanceBuilder& InstanceBuilder::engine_name(std::string engine_name) {
    engine_name_ = std::move(engine_name);
    return *this;
}

InstanceBuilder& InstanceBuilder::engine_version(std::uint32_t engine_version) {
    engine_version_ = engine_version;
    return *this;
}

InstanceBuilder& InstanceBuilder::api_version(std::uint32_t api_version) {
    api_version_ = api_version;
    return *this;
}

InstanceBuilder& InstanceBuilder::flags(vk::InstanceCreateFlags flags) {
    flags_ = flags;
    return *this;
}
