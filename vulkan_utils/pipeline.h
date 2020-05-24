#pragma once
#include <vector>
#include "vulkan/vulkan.hpp"

class PipelineLayoutBuilder{
public:
    PipelineLayoutBuilder& flags(vk::PipelineLayoutCreateFlags val);
    PipelineLayoutBuilder& set_layouts(std::vector<vk::DescriptorSetLayout> val);
    PipelineLayoutBuilder& push_constant_ranges(std::vector<vk::PushConstantRange> val);

    vk::UniquePipelineLayout build(vk::UniqueDevice& device);
private:
    vk::PipelineLayoutCreateFlags flags_{};
    std::vector<vk::DescriptorSetLayout> set_layouts_{};
    std::vector<vk::PushConstantRange> push_constant_ranges_{};
};


class RenderPassBuilder {
public:
    RenderPassBuilder& flags(vk::RenderPassCreateFlags flags);
    RenderPassBuilder& attachments(std::vector<vk::AttachmentDescription> attachments);
    RenderPassBuilder& subpasses(std::vector<vk::SubpassDescription> subpasses);
    RenderPassBuilder& dependencies(std::vector<vk::SubpassDependency> dependencies);

    vk::UniqueRenderPass build(vk::UniqueDevice& device);
private:
    vk::RenderPassCreateFlags flags_ {};
    std::vector<vk::AttachmentDescription> attachments_ {};
    std::vector<vk::SubpassDescription> subpasses_ {};
    std::vector<vk::SubpassDependency> dependencies_ {};
};

