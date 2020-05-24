#include "swapchain.h"

SwapchainBuilder& SwapchainBuilder::flags(vk::SwapchainCreateFlagsKHR val) {
    return *this;
}

SwapchainBuilder& SwapchainBuilder::surface(vk::UniqueSurfaceKHR& val) {
    surface_ = *val;
    return *this;
}

SwapchainBuilder& SwapchainBuilder::min_image_count(uint32_t val) {
    min_image_count_ = val;
    return *this;
}

SwapchainBuilder& SwapchainBuilder::image_format(vk::Format val) {
    image_format_ = val;
    return *this;
}

SwapchainBuilder& SwapchainBuilder::image_color_space(vk::ColorSpaceKHR val) {
    image_color_space_ = val;
    return *this;
}

SwapchainBuilder& SwapchainBuilder::image_extent(vk::Extent2D val) {
    image_extent_ = val;
    return *this;
}

SwapchainBuilder& SwapchainBuilder::image_array_layers(uint32_t val) {
    image_array_layers_ = val;
    return *this;
}

SwapchainBuilder& SwapchainBuilder::image_usage(vk::ImageUsageFlags val) {
    image_usage_ = val;
    return *this;
}

SwapchainBuilder& SwapchainBuilder::image_sharing_mode(vk::SharingMode val) {
    image_sharing_mode_ = val;
    return *this;
}

SwapchainBuilder& SwapchainBuilder::queue_family_indices(std::vector<uint32_t> val) {
    queue_family_indices_ = val;
    return *this;
}

SwapchainBuilder& SwapchainBuilder::pre_transform(vk::SurfaceTransformFlagBitsKHR val) {
    pre_transform_ = val;
    return *this;
}

SwapchainBuilder& SwapchainBuilder::composite_alpha(vk::CompositeAlphaFlagBitsKHR val) {
    composite_alpha_ = val;
    return *this;
}

SwapchainBuilder& SwapchainBuilder::present_mode(vk::PresentModeKHR val) {
    present_mode_ = val;
    return *this;
}

SwapchainBuilder& SwapchainBuilder::clipped(vk::Bool32 val) {
    clipped_ = val;
    return *this;
}

SwapchainBuilder& SwapchainBuilder::old_swapchain(vk::UniqueSwapchainKHR val) {
    old_swapchain_ = std::move(val);
    return *this;
}

vk::UniqueSwapchainKHR SwapchainBuilder::build(vk::UniqueDevice& device) {
    const auto old_swapchain = [&]{
        if (old_swapchain_) {
            return *old_swapchain_;
        }
        return vk::SwapchainKHR{};
    }();

    vk::SwapchainCreateInfoKHR create_info{
        flags_,
        surface_,
        min_image_count_,
        image_format_,
        image_color_space_,
        image_extent_,
        image_array_layers_,
        image_usage_,
        image_sharing_mode_,
        static_cast<uint32_t>(queue_family_indices_.size()),
        queue_family_indices_.data(),
        pre_transform_,
        composite_alpha_,
        present_mode_,
        clipped_,
        old_swapchain
    };
    return device->createSwapchainKHRUnique(create_info);
}


ImageViewBuilder& ImageViewBuilder::flags(vk::ImageViewCreateFlags val) {
    flags_ = val;
    return *this;
}

ImageViewBuilder& ImageViewBuilder::image(vk::Image val) {
    image_ = val;
    return *this;
}

ImageViewBuilder& ImageViewBuilder::view_type(vk::ImageViewType val) {
    view_type_ = val;
    return *this;
}

ImageViewBuilder& ImageViewBuilder::format(vk::Format val) {
    format_ = val;
    return *this;
}

ImageViewBuilder& ImageViewBuilder::components(vk::ComponentMapping val) {
    components_ = val;
    return *this;
}

ImageViewBuilder& ImageViewBuilder::subresource_range(vk::ImageSubresourceRange val) {
    subresource_range_ = val;
    return *this;
}

vk::UniqueImageView ImageViewBuilder::build(vk::UniqueDevice& device) {
    vk::ImageViewCreateInfo create_info {
        flags_,
        image_,
        view_type_,
        format_,
        components_,
        subresource_range_
    };

    return device->createImageViewUnique(create_info);
}
