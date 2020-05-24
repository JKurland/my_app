#pragma once
#include <vector>
#include <cstdint>

#include "vulkan/vulkan.hpp"


class InstanceBuilder {
public:
    vk::UniqueInstance build();

    InstanceBuilder& app_name(std::string app_name);
    InstanceBuilder& app_version(std::uint32_t app_version);
    InstanceBuilder& engine_name(std::string engine_name);
    InstanceBuilder& engine_version(std::uint32_t engine_version);
    InstanceBuilder& api_version(std::uint32_t api_version);

    InstanceBuilder& flags(vk::InstanceCreateFlags flags);
    InstanceBuilder& extensions(std::vector<std::string> extensions);
    InstanceBuilder& layers(std::vector<std::string> layers);

private:
    std::vector<std::string> extensions_{};
    std::vector<std::string> layers_{};
    std::string app_name_{};
    std::uint32_t app_version_{};
    std::string engine_name_{};
    std::uint32_t engine_version_{};
    std::uint32_t api_version_{};
    vk::InstanceCreateFlags flags_{};
};
