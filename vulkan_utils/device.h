#pragma once

#include <string>
#include <vector>
#include "vulkan/vulkan.hpp"

class DeviceQueueBuilder {
public:
    DeviceQueueBuilder& flags(vk::DeviceQueueCreateFlags flags);
    DeviceQueueBuilder& family_index(std::uint32_t queue_family_index);
    DeviceQueueBuilder& priorities(std::vector<float> queue_priorities);
private:
    vk::DeviceQueueCreateInfo build() const;
    vk::DeviceQueueCreateFlags flags_{};
    std::uint32_t queue_family_index_{};
    std::vector<float> queue_priorities_{};

    friend class DeviceBuilder;
};

class DeviceBuilder {
public:
    DeviceBuilder& flags(vk::DeviceCreateFlags flags);
    DeviceBuilder& queue_create_infos(std::vector<DeviceQueueBuilder> queue_create_infos);
    DeviceBuilder& layers(std::vector<std::string> layers);
    DeviceBuilder& extensions(std::vector<std::string> extensions);
    DeviceBuilder& features(vk::PhysicalDeviceFeatures features);
    vk::UniqueDevice build(const vk::PhysicalDevice& pd) const;
private:
    vk::DeviceCreateFlags flags_{};
    std::vector<DeviceQueueBuilder> queue_create_infos_{};
    std::vector<std::string> layers_{};
    std::vector<std::string> extensions_{};
    vk::PhysicalDeviceFeatures features_{};
};
