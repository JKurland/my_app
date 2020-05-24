#include "device.h"

namespace {
    std::vector<const char*> c_strs(const std::vector<std::string>& strs) {
        std::vector<const char*> rtn;
        for (const auto& str: strs) {
            rtn.push_back(str.c_str());
        }
        return rtn;
    }
}

DeviceQueueBuilder& DeviceQueueBuilder::flags(vk::DeviceQueueCreateFlags flags) {
    flags_ = std::move(flags);
    return *this;
}

DeviceQueueBuilder& DeviceQueueBuilder::family_index(std::uint32_t queue_family_index) {
    queue_family_index_ = std::move(queue_family_index);
    return *this;
}

DeviceQueueBuilder& DeviceQueueBuilder::priorities(std::vector<float> queue_priorities) {
    queue_priorities_ = std::move(queue_priorities);
    return *this;
}

vk::DeviceQueueCreateInfo DeviceQueueBuilder::build() const {
    return vk::DeviceQueueCreateInfo(flags_, queue_family_index_, queue_priorities_.size(), queue_priorities_.data());
}


DeviceBuilder& DeviceBuilder::flags(vk::DeviceCreateFlags flags) {
    flags_ = std::move(flags);
    return *this;
}

DeviceBuilder& DeviceBuilder::queue_create_infos(std::vector<DeviceQueueBuilder> queue_create_infos) {
    queue_create_infos_ = std::move(queue_create_infos);
    return *this;
}

DeviceBuilder& DeviceBuilder::layers(std::vector<std::string> layers) {
    layers_ = std::move(layers);
    return *this;
}

DeviceBuilder& DeviceBuilder::extensions(std::vector<std::string> extensions) {
    extensions_ = std::move(extensions);
    return *this;
}

DeviceBuilder& DeviceBuilder::features(vk::PhysicalDeviceFeatures features) {
    features_ = std::move(features);
    return *this;
}

vk::UniqueDevice DeviceBuilder::build(const vk::PhysicalDevice& pd) const {
    std::vector<vk::DeviceQueueCreateInfo> q_create_infos;
    for (const auto& b: queue_create_infos_) {
        q_create_infos.push_back(b.build());
    }

    auto layer_c_strs = c_strs(layers_);
    auto extension_c_strs = c_strs(extensions_);

    vk::DeviceCreateInfo create_info(
        flags_,
        q_create_infos.size(),
        q_create_infos.data(),
        layer_c_strs.size(),
        layer_c_strs.data(),
        extension_c_strs.size(),
        extension_c_strs.data(),
        &features_
    );

    return pd.createDeviceUnique(create_info);
}
