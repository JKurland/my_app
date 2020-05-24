#pragma once
#include "vulkan/vulkan.hpp"


class SwapchainBuilder {
public:
    SwapchainBuilder& flags(vk::SwapchainCreateFlagsKHR val);
    SwapchainBuilder& surface(vk::UniqueSurfaceKHR& val);
    SwapchainBuilder& min_image_count(uint32_t val);
    SwapchainBuilder& image_format(vk::Format val);
    SwapchainBuilder& image_color_space(vk::ColorSpaceKHR val);
    SwapchainBuilder& image_extent(vk::Extent2D val);
    SwapchainBuilder& image_array_layers(uint32_t val);
    SwapchainBuilder& image_usage(vk::ImageUsageFlags val);
    SwapchainBuilder& image_sharing_mode(vk::SharingMode val);
    SwapchainBuilder& queue_family_indices(std::vector<uint32_t> val);
    SwapchainBuilder& pre_transform(vk::SurfaceTransformFlagBitsKHR val);
    SwapchainBuilder& composite_alpha(vk::CompositeAlphaFlagBitsKHR val);
    SwapchainBuilder& present_mode(vk::PresentModeKHR val);
    SwapchainBuilder& clipped(vk::Bool32 val);
    SwapchainBuilder& old_swapchain(vk::UniqueSwapchainKHR val);

    vk::UniqueSwapchainKHR build(vk::UniqueDevice& device);

private:
    vk::SwapchainCreateFlagsKHR flags_{};
    vk::SurfaceKHR surface_{};
    uint32_t min_image_count_{};
    vk::Format image_format_{};
    vk::ColorSpaceKHR image_color_space_{};
    vk::Extent2D image_extent_{};
    uint32_t image_array_layers_{};
    vk::ImageUsageFlags image_usage_{};
    vk::SharingMode image_sharing_mode_{};
    std::vector<uint32_t> queue_family_indices_{};
    vk::SurfaceTransformFlagBitsKHR pre_transform_{};
    vk::CompositeAlphaFlagBitsKHR composite_alpha_{};
    vk::PresentModeKHR present_mode_{};
    vk::Bool32 clipped_{};
    vk::UniqueSwapchainKHR old_swapchain_{};
};


class ImageViewBuilder {
public:
    ImageViewBuilder& flags(vk::ImageViewCreateFlags val);
    ImageViewBuilder& image(vk::Image val);
    ImageViewBuilder& view_type(vk::ImageViewType val);
    ImageViewBuilder& format(vk::Format val);
    ImageViewBuilder& components(vk::ComponentMapping val);
    ImageViewBuilder& subresource_range(vk::ImageSubresourceRange val);

    vk::UniqueImageView build(vk::UniqueDevice& device);

private:
    vk::ImageViewCreateFlags flags_ {};
    vk::Image image_ {};
    vk::ImageViewType view_type_ = vk::ImageViewType::e1D;
    vk::Format format_ = vk::Format::eUndefined;
    vk::ComponentMapping components_ {};
    vk::ImageSubresourceRange subresource_range_ {};
};
