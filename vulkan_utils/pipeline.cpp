#include "pipeline.h"

PipelineLayoutBuilder& PipelineLayoutBuilder::flags(vk::PipelineLayoutCreateFlags val) {
    flags_ = val;
    return *this;
}
PipelineLayoutBuilder& PipelineLayoutBuilder::set_layouts(std::vector<vk::DescriptorSetLayout> val) {
    set_layouts_ = val;
    return *this;
}
PipelineLayoutBuilder& PipelineLayoutBuilder::push_constant_ranges(std::vector<vk::PushConstantRange> val) {
    push_constant_ranges_ = val;
    return *this;
}

vk::UniquePipelineLayout PipelineLayoutBuilder::build(vk::UniqueDevice& device) {
    return device->createPipelineLayoutUnique(
        vk::PipelineLayoutCreateInfo{}
            .setSetLayoutCount(set_layouts_.size())
            .setPSetLayouts(set_layouts_.data())
            .setPushConstantRangeCount(push_constant_ranges_.size())
            .setPPushConstantRanges(push_constant_ranges_.data())
    );
}


RenderPassBuilder& RenderPassBuilder::flags(vk::RenderPassCreateFlags flags) {
    flags_ = flags;
    return *this;
}

RenderPassBuilder& RenderPassBuilder::attachments(std::vector<vk::AttachmentDescription> attachments) {
    attachments_ = attachments;
    return *this;
}

RenderPassBuilder& RenderPassBuilder::subpasses(std::vector<vk::SubpassDescription> subpasses) {
    subpasses_ = subpasses;
    return *this;
}

RenderPassBuilder& RenderPassBuilder::dependencies(std::vector<vk::SubpassDependency> dependencies) {
    dependencies_ = dependencies;
    return *this;
}


vk::UniqueRenderPass RenderPassBuilder::build(vk::UniqueDevice& device) {
    return device->createRenderPassUnique(
        vk::RenderPassCreateInfo{}
            .setAttachmentCount(attachments_.size())
            .setPAttachments(attachments_.data())
            .setSubpassCount(subpasses_.size())
            .setPSubpasses(subpasses_.data())
            .setDependencyCount(dependencies_.size())
            .setPDependencies(dependencies_.data())
    );
}