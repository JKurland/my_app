#include <iostream>
#include <optional>
#include <string>
#include <exception>
#include <memory>
#include <fstream>
#include <array>
#include <limits>
#include <cstddef>
#include <chrono>

#include "event/serial.h"
#include "event/first.h"
#include "event/buffered.h"
#include "event/must_handle.h"
#include "vulkan_utils/instance.h"
#include "vulkan_utils/device.h"
#include "vulkan_utils/swapchain.h"
#include "vulkan_utils/pipeline.h"

#include "SDL2/SDL.h"
#include "SDL2/SDL_vulkan.h"
#include "vulkan/vulkan.hpp"

#define GLM_FORCE_RADIANS
#include "glm/glm.hpp"
#include "glm/gtc/matrix_transform.hpp"

#define STB_IMAGE_IMPLEMENTATION
#include "stb/stb_image.h"


namespace {
    std::vector<char> read_file(const std::string& filename) {
        std::ifstream file(filename, std::ios::ate | std::ios::binary);

        if (!file.is_open()) {
            throw std::runtime_error("failed to open file: " + filename);
        }

        std::size_t file_size = static_cast<std::size_t>(file.tellg());
        std::vector<char> buffer(file_size);
        file.seekg(0);
        file.read(buffer.data(), file_size);
        return buffer;
    }

    vk::UniqueShaderModule create_shader_module(vk::UniqueDevice& device, const std::vector<char>& code) {
        return device->createShaderModuleUnique(
            vk::ShaderModuleCreateInfo{}
                .setCodeSize(code.size())
                .setPCode(reinterpret_cast<const uint32_t*>(code.data()))
        );
    }
}

class StbImage {
public:
    static StbImage from_file(const std::string& filename) {
        int width;
        int height;
        int channels;
        stbi_uc* pixels = stbi_load(filename.c_str(), &width, &height, &channels, STBI_rgb_alpha);
        if (!pixels) {
            throw std::runtime_error("Failed to load image from file");
        }
        return StbImage(pixels, static_cast<uint32_t>(width), static_cast<uint32_t>(height), channels);
    }

    size_t width() const {return width_;}
    size_t height() const {return height_;}
    vk::Extent2D extent() const {return {width_, height_};}
    size_t channels() const {return channels_;}
    size_t size() const {return width_ * height_ * 4;}

    stbi_uc* pixels() {return pixels_.get();}
    const stbi_uc* pixels() const {return pixels_.get();}
private:
    struct Deleter {
        void operator()(stbi_uc* data) {
            stbi_image_free(data);
        }
    };

    std::unique_ptr<stbi_uc, Deleter> pixels_;
    uint32_t width_;
    uint32_t height_;
    int channels_;
    StbImage(stbi_uc* pixels, uint32_t width, uint32_t height, int channels):
        pixels_(pixels),
        width_(width),
        height_(height),
        channels_(channels) {}
};

class Window {
public:
    template<typename...Args>
    Window(Args&&...args): window(SDL_CreateWindow(std::forward<Args>(args)...)) {
        if(!window) {
            throw std::runtime_error("Failed to create window");
        }
    }

    void close() {window.reset();}

    SDL_Window* get() {return window.get();}
    const SDL_Window* get() const {return window.get();}

    operator bool() const {return static_cast<bool>(window);}

    std::vector<std::string> vulkan_extensions() {
        uint32_t extensionCount;
        SDL_Vulkan_GetInstanceExtensions(window.get(), &extensionCount, nullptr);
        std::vector<const char *> extension_names(extensionCount);
        SDL_Vulkan_GetInstanceExtensions(window.get(), &extensionCount, extension_names.data());

        std::vector<std::string> rtn;
        for (const auto& name: extension_names) {
            rtn.emplace_back(name);
        }
        return rtn;
    }


    vk::UniqueSurfaceKHR vulkan_surface(vk::UniqueInstance& instance) {
        VkSurfaceKHR surface_raw;
        if (!SDL_Vulkan_CreateSurface(get(), *instance, &surface_raw)) {
            throw std::runtime_error("Failed to create surface");
        }
        return vk::UniqueSurfaceKHR(
            vk::SurfaceKHR(surface_raw),
            vk::ObjectDestroy<
                vk::Instance,
                decltype(VULKAN_HPP_DEFAULT_DISPATCHER)
            >(*instance)
        );
    }

private:
    struct WindowDelete {
        void operator()(SDL_Window* ptr) {
            if (ptr) {
                SDL_DestroyWindow(ptr);
            }
        }
    };

    std::unique_ptr<SDL_Window, WindowDelete> window;
};

template<typename EventHandlerT, typename RequestHandlerT>
class Ctx {
public:
    Ctx(EventHandlerT event_handler, RequestHandlerT request_handler):
        event_handler(std::move(event_handler)),
        request_handler(MustHandle{std::move(request_handler)}){}

    template<class EventT>
    void handle_event(EventT&& t) {
        event_handler(*this, std::forward<EventT>(t));
    }

    template<class RequestT>
    auto handle_request(RequestT&& t) {
        return request_handler(*this, std::forward<RequestT>(t));
    }
private:
    EventHandlerT event_handler;
    MustHandle<RequestHandlerT> request_handler;
};


struct MyEvent {
    MyEvent(const MyEvent&) = delete;
    MyEvent& operator=(const MyEvent&) = delete;

    MyEvent(MyEvent&&) = default;
    MyEvent& operator=(MyEvent&&) = default;
};

struct MyRequest {
    MyRequest(const MyRequest&) = delete;
    MyRequest& operator=(const MyRequest&) = delete;

    MyRequest(MyRequest&&) = default;
    MyRequest& operator=(MyRequest&&) = default;
};

struct Vertex {
    glm::vec2 pos;
    glm::vec3 colour;

    constexpr static auto binding_description() {
        return vk::VertexInputBindingDescription {
            0,
            sizeof(Vertex),
            vk::VertexInputRate::eVertex,
        };
    }

    constexpr static auto attribute_description() {
        return std::array<vk::VertexInputAttributeDescription, 2> {
            vk::VertexInputAttributeDescription {
                0,
                0,
                vk::Format::eR32G32Sfloat,
                offsetof(Vertex, pos),
            },
            vk::VertexInputAttributeDescription {
                1,
                0,
                vk::Format::eR32G32B32Sfloat,
                offsetof(Vertex, colour),
            },
        };
    }
};

struct Mvp {
    glm::mat4 model;
    glm::mat4 view;
    glm::mat4 proj;
};

struct SpinAnimation {
    std::chrono::steady_clock::time_point start;
    float degrees_per_second;

    glm::mat4 model() const {
        auto now = std::chrono::steady_clock::now();
        float time = std::chrono::duration<float, std::chrono::seconds::period>(now - start).count();
        return glm::rotate(glm::mat4(1.0f), time * glm::radians(degrees_per_second), glm::vec3(0.0f, 0.0f, 1.0f));
    }
};


const std::vector<Vertex> kVertices = {
    {{-0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}},
    {{-0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}},
    {{0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}},
    {{0.5f, 0.5f}, {0.0f, 1.0f, 0.0f}},
};

const std::vector<uint16_t> kIndices = {
    0, 1, 2, 0, 2, 3
};


class VulkanState {
public:
    static VulkanState for_window(Window& window) {
        VulkanState vulkan_state{};
        auto extensions = window.vulkan_extensions();

        vulkan_state.instance = InstanceBuilder()
                        .app_name("AppName")
                        .app_version(1)
                        .engine_name("Vulkan.hpp")
                        .engine_version(1)
                        .api_version(VK_API_VERSION_1_1)
                        .extensions(extensions)
                        .build();

        vulkan_state.surface = window.vulkan_surface(vulkan_state.instance);

        vulkan_state.queue_family_index = 0;
        vulkan_state.device = DeviceBuilder()
                        .queue_create_infos({
                            DeviceQueueBuilder()
                            .family_index(vulkan_state.queue_family_index)
                            .priorities({1.0f})
                        })
                        .extensions({
                            VK_KHR_SWAPCHAIN_EXTENSION_NAME
                        })
                        .build(vulkan_state.physical_device());

        vulkan_state.setup_swapchain();
        return vulkan_state;
    }

    int queue_family_index;

    vk::UniqueInstance instance;
    vk::UniqueSurfaceKHR surface;
    vk::UniqueDevice device;
    vk::UniqueSwapchainKHR swapchain;
    vk::UniqueShaderModule vert_module;
    vk::UniqueShaderModule frag_module;
    std::vector<vk::UniqueImageView> image_views;
    vk::UniqueDescriptorSetLayout descriptor_set_layout;
    vk::UniquePipelineLayout pipeline_layout;
    vk::UniqueRenderPass render_pass;
    vk::UniquePipeline graphics_pipeline;
    std::vector<vk::UniqueFramebuffer> framebuffers;
    vk::UniqueCommandPool command_pool;

    vk::UniqueBuffer vertex_buffer;
    vk::UniqueDeviceMemory vertex_buffer_memory;

    vk::UniqueBuffer index_buffer;
    vk::UniqueDeviceMemory index_buffer_memory;

    vk::UniqueImage texture_image;
    vk::UniqueDeviceMemory texture_image_memory;

    vk::UniqueDescriptorPool descriptor_pool;
    std::vector<vk::UniqueDescriptorSet> descriptor_sets;
    std::vector<vk::UniqueBuffer> uniform_buffers;
    std::vector<vk::UniqueDeviceMemory> uniform_buffers_memory;

    std::vector<vk::UniqueCommandBuffer> command_buffers;
    std::vector<vk::UniqueSemaphore> image_available_semaphores;
    std::vector<vk::UniqueSemaphore> render_finished_semaphores;
    std::vector<vk::UniqueFence> in_flight_fences;
    std::vector<vk::Fence> image_fences;

    vk::Extent2D current_extent() const {
        return physical_device().getSurfaceCapabilitiesKHR(*surface).currentExtent;
    }

    vk::PhysicalDevice physical_device() const {
        return instance->enumeratePhysicalDevices().front();
    }

    size_t vertex_buffer_size() const {
        return sizeof(kVertices[0]) * kVertices.size();
    }

    size_t index_buffer_size() const {
        return sizeof(kIndices[0]) * kIndices.size();
    }

    vk::Queue queue() {
        return device->getQueue(queue_family_index, 0);
    }

    struct StagingBuffer {
        static StagingBuffer from_data(VulkanState& vulkan_state, const void* data, size_t size) {
            StagingBuffer sb;
            sb.buffer = vulkan_state.make_buffer(vk::BufferUsageFlagBits::eTransferSrc, size);
            sb.memory = vulkan_state.make_buffer_memory(
                *sb.buffer,
                vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent
            );

            void* dst_mem = vulkan_state.device->mapMemory(*sb.memory, 0, size, vk::MemoryMapFlags{});
            memcpy(dst_mem, data, size);
            vulkan_state.device->unmapMemory(*sb.memory);
            return sb;
        }
        vk::UniqueBuffer buffer;
        vk::UniqueDeviceMemory memory;
    };

    void copy_data_to_buffer(vk::Buffer dst, const void* data, size_t size) {
        copy_buffer(
            *(StagingBuffer::from_data(*this, data, size).buffer),
            dst,
            size
        );
    }

    void copy_stbimage_to_image(vk::Image dst, const StbImage& image) {
        transition_image_layout(
            dst,
            vk::Format::eR8G8B8A8Srgb,
            vk::ImageLayout::eUndefined,
            vk::ImageLayout::eTransferDstOptimal
        );

        copy_buffer_to_image(
            *(StagingBuffer::from_data(*this, image.pixels(), image.size()).buffer),
            dst,
            image.extent()
        );
    }

    void copy_data_to_vertex_buffer(const std::vector<Vertex>& vertices) {
        if (sizeof(vertices[0])*vertices.size() > vertex_buffer_size()) {
            throw std::runtime_error("Failed to copy vertices too much data to copy");
        }
        copy_data_to_buffer(*vertex_buffer, vertices.data(), vertices.size() * sizeof(vertices[0]));
    }

    void copy_data_to_index_buffer(const std::vector<uint16_t>& indices) {
        if (sizeof(indices[0])*indices.size() > index_buffer_size()) {
            throw std::runtime_error("Failed to copy indices too much data to copy");
        }
        copy_data_to_buffer(*index_buffer, indices.data(), indices.size() * sizeof(indices[0]));
    }

    vk::UniqueCommandBuffer begin_one_time_commands() {
        auto command_buffers = device->allocateCommandBuffersUnique(
            vk::CommandBufferAllocateInfo{}
                .setCommandPool(*command_pool)
                .setLevel(vk::CommandBufferLevel::ePrimary)
                .setCommandBufferCount(1)
        );

        command_buffers[0]->begin(
            vk::CommandBufferBeginInfo{}
                .setFlags(vk::CommandBufferUsageFlagBits::eOneTimeSubmit)
        );

        return std::move(command_buffers[0]);
    }

    void end_one_time_commands(vk::UniqueCommandBuffer command_buffer) {
        command_buffer->end();

        auto queue = this->queue();
        queue.submit(
            vk::SubmitInfo{}
                .setCommandBufferCount(1)
                .setPCommandBuffers(&command_buffer.get()),
            vk::Fence{}
        );
        queue.waitIdle();
    }

    void copy_buffer(vk::Buffer src, vk::Buffer dst, vk::DeviceSize size) {
        auto command_buffer = begin_one_time_commands();
        command_buffer->copyBuffer(
            src,
            dst,
            vk::BufferCopy{0, 0, size}
        );
        end_one_time_commands(std::move(command_buffer));
    }

    void copy_buffer_to_image(vk::Buffer src, vk::Image dst, vk::Extent2D extent) {
        auto command_buffer = begin_one_time_commands();
        command_buffer->copyBufferToImage(
            src,
            dst,
            vk::ImageLayout::eTransferDstOptimal,
            vk::BufferImageCopy{}
                .setBufferOffset(0)
                .setBufferRowLength(0)
                .setBufferImageHeight(0)
                .setImageSubresource(vk::ImageSubresourceLayers{}
                    .setAspectMask(vk::ImageAspectFlagBits::eColor)
                    .setMipLevel(0)
                    .setBaseArrayLayer(0)
                    .setLayerCount(1))
                .setImageOffset({0, 0, 0})
                .setImageExtent({extent.width, extent.height, 1})
        );
        end_one_time_commands(std::move(command_buffer));
    }

    void transition_image_layout(
        vk::Image image,
        vk::Format format,
        vk::ImageLayout old_layout,
        vk::ImageLayout new_layout
    ) {

        auto command_buffer = begin_one_time_commands();
        auto barrier = vk::ImageMemoryBarrier{}
            .setOldLayout(old_layout)
            .setNewLayout(new_layout)
            .setSrcQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
            .setDstQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
            .setImage(image)
            .setSubresourceRange(vk::ImageSubresourceRange{}
                .setAspectMask(vk::ImageAspectFlagBits::eColor)
                .setBaseMipLevel(0)
                .setLevelCount(1)
                .setBaseArrayLayer(0)
                .setLayerCount(1));
        
        vk::PipelineStageFlags source_stage;
        vk::PipelineStageFlags destination_stage;

        if (old_layout == vk::ImageLayout::eUndefined && new_layout == vk::ImageLayout::eTransferDstOptimal) {
            barrier.setSrcAccessMask({})
                   .setDstAccessMask(vk::AccessFlagBits::eTransferWrite);
            
            source_stage = vk::PipelineStageFlagBits::eTopOfPipe;
            destination_stage = vk::PipelineStageFlagBits::eTransfer;
        } else if (old_layout == vk::ImageLayout::eTransferDstOptimal && new_layout == vk::ImageLayout::eShaderReadOnlyOptimal) {
            barrier.setSrcAccessMask(vk::AccessFlagBits::eTransferWrite)
                   .setDstAccessMask(vk::AccessFlagBits::eShaderRead);
            
            source_stage = vk::PipelineStageFlagBits::eTransfer;
            destination_stage = vk::PipelineStageFlagBits::eFragmentShader;
        } else {
            throw std::invalid_argument("unsupported layout transition");
        }

        command_buffer->pipelineBarrier(source_stage, destination_stage, {}, {}, {}, barrier);
        end_one_time_commands(std::move(command_buffer));
    }

    uint32_t find_memory_type(uint32_t type_filter, vk::MemoryPropertyFlags properties) const {
        auto mem_properties = physical_device().getMemoryProperties();
        for (size_t i = 0; i < mem_properties.memoryTypeCount; i++) {
            if (!(type_filter & (1<<i))) {
                continue;
            }

            if ((mem_properties.memoryTypes[i].propertyFlags & properties) == properties) {
                return i;
            }
        }

        throw std::runtime_error("Failed to find suitable memory type");
    }

    vk::UniqueBuffer make_buffer(vk::BufferUsageFlags usage, vk::DeviceSize size) {
        return device->createBufferUnique(
            vk::BufferCreateInfo{}
                .setSize(size)
                .setUsage(usage)
                .setSharingMode(vk::SharingMode::eExclusive)
        );
    }

    vk::UniqueDeviceMemory make_buffer_memory(vk::Buffer buffer, vk::MemoryPropertyFlags properties) {
        const auto mem_requirements = device->getBufferMemoryRequirements(buffer);
        auto buffer_memory = device->allocateMemoryUnique(
            vk::MemoryAllocateInfo {
                mem_requirements.size,
                find_memory_type(
                    mem_requirements.memoryTypeBits,
                    properties
                )
            }
        );
        device->bindBufferMemory(buffer, *buffer_memory, 0);
        return buffer_memory;
    }

    vk::UniqueImage make_image(
        vk::Extent2D extent,
        vk::Format format,
        vk::ImageTiling tiling,
        vk::ImageUsageFlags usage
    ) {
        return device->createImageUnique(
            vk::ImageCreateInfo{}
                .setImageType(vk::ImageType::e2D)
                .setExtent(vk::Extent3D{extent.width, extent.height, 1})
                .setMipLevels(1)
                .setArrayLayers(1)
                .setFormat(format)
                .setTiling(tiling)
                .setInitialLayout(vk::ImageLayout::eUndefined)
                .setUsage(usage)
                .setSharingMode(vk::SharingMode::eExclusive)
                .setSamples(vk::SampleCountFlagBits::e1)
        );
    }

    vk::UniqueDeviceMemory make_image_memory(vk::Image image, vk::MemoryPropertyFlags properties) {
        const auto mem_requirements = device->getImageMemoryRequirements(image);
        auto image_memory = device->allocateMemoryUnique(
            vk::MemoryAllocateInfo(
                mem_requirements.size,
                find_memory_type(
                    mem_requirements.memoryTypeBits,
                    properties
                )
            )
        );
        device->bindImageMemory(image, *image_memory, 0);
        return image_memory;
    }

    void update_uniform_buffer(const SpinAnimation& animation, uint32_t image_index) {
        const auto extent = current_extent();
        Mvp mvp{
            animation.model(),
            glm::lookAt(glm::vec3(2.0f, 2.0f, 2.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f)),
            glm::perspective(glm::radians(45.0f), extent.width / static_cast<float>(extent.height), 0.1f, 10.0f),
        };
        mvp.proj[1][1] *= -1;

        void* data = device->mapMemory(*uniform_buffers_memory[image_index], 0, sizeof(mvp));
        memcpy(data, &mvp, sizeof(mvp));
        device->unmapMemory(*uniform_buffers_memory[image_index]);
    }

    void setup_swapchain() {
        vk::Extent2D extent = current_extent();
        const auto image_count = 3;
        const auto image_format = vk::Format::eB8G8R8A8Srgb;
        swapchain = SwapchainBuilder()
                        .min_image_count(image_count)
                        .surface(surface)
                        .image_format(image_format)
                        .image_color_space(vk::ColorSpaceKHR::eSrgbNonlinear)
                        .image_extent(extent)
                        .image_array_layers(1)
                        .image_usage(vk::ImageUsageFlagBits::eColorAttachment)
                        .image_sharing_mode(vk::SharingMode::eExclusive)
                        .pre_transform(vk::SurfaceTransformFlagBitsKHR::eIdentity)
                        .composite_alpha(vk::CompositeAlphaFlagBitsKHR::eOpaque)
                        .present_mode(vk::PresentModeKHR::eFifo)
                        .clipped(true)
                        .build(device);

        auto images = device->getSwapchainImagesKHR(*swapchain);
        if (images.size() < image_count) throw std::runtime_error("Not enough images");

        for (int i = 0; i < image_count; i++) {
            image_views.push_back(
                ImageViewBuilder()
                .image(images[i])
                .view_type(vk::ImageViewType::e2D)
                .format(image_format)
                .components(
                    vk::ComponentMapping{}
                    .setR(vk::ComponentSwizzle::eIdentity)
                    .setG(vk::ComponentSwizzle::eIdentity)
                    .setB(vk::ComponentSwizzle::eIdentity)
                    .setA(vk::ComponentSwizzle::eIdentity)
                )
                .subresource_range(
                    vk::ImageSubresourceRange{}
                    .setAspectMask(vk::ImageAspectFlagBits::eColor)
                    .setBaseMipLevel(0)
                    .setLevelCount(1)
                    .setBaseArrayLayer(0)
                    .setLayerCount(1)
                )
                .build(device)
            );
        }
        std::vector<vk::PipelineShaderStageCreateInfo> shader_stages;

        const auto vert_shader = read_file("test.vert.spv");
        vert_module = create_shader_module(device, vert_shader);
        shader_stages.push_back(
            vk::PipelineShaderStageCreateInfo{}
            .setStage(vk::ShaderStageFlagBits::eVertex)
            .setModule(*vert_module)
            .setPName("main")
        );
        

        const auto frag_shader = read_file("test.frag.spv");
        frag_module = create_shader_module(device, frag_shader);
        shader_stages.push_back(
            vk::PipelineShaderStageCreateInfo{}
            .setStage(vk::ShaderStageFlagBits::eFragment)
            .setModule(*frag_module)
            .setPName("main")
        );

        auto ubo_layout_binding = vk::DescriptorSetLayoutBinding{}
            .setBinding(0)
            .setDescriptorType(vk::DescriptorType::eUniformBuffer)
            .setDescriptorCount(1)
            .setStageFlags(vk::ShaderStageFlagBits::eVertex);
        
        if (!descriptor_set_layout) {
            descriptor_set_layout = device->createDescriptorSetLayoutUnique(
                vk::DescriptorSetLayoutCreateInfo{}
                    .setBindingCount(1)
                    .setPBindings(&ubo_layout_binding)
            );
        }

        auto vertex_binding_description = Vertex::binding_description();
        auto vertex_attribute_description = Vertex::attribute_description();
        auto vertex_input_create_info = vk::PipelineVertexInputStateCreateInfo{}
            .setVertexBindingDescriptionCount(1)
            .setPVertexBindingDescriptions(&vertex_binding_description)
            .setVertexAttributeDescriptionCount(static_cast<uint32_t>(vertex_attribute_description.size()))
            .setPVertexAttributeDescriptions(vertex_attribute_description.data());

        auto assembly_create_info = vk::PipelineInputAssemblyStateCreateInfo{}
            .setTopology(vk::PrimitiveTopology::eTriangleList)
            .setPrimitiveRestartEnable(false);

        auto viewport = vk::Viewport{}
            .setX(0.0f)
            .setY(0.0f)
            .setWidth(static_cast<float>(extent.width))
            .setHeight(static_cast<float>(extent.height))
            .setMinDepth(0.0f)
            .setMaxDepth(1.0f);

        auto scissor = vk::Rect2D{}
            .setOffset(vk::Offset2D{0, 0})
            .setExtent(extent);

        auto viewport_state = vk::PipelineViewportStateCreateInfo{}
            .setViewportCount(1)
            .setPViewports(&viewport)
            .setScissorCount(1)
            .setPScissors(&scissor);

        auto rasteriser = vk::PipelineRasterizationStateCreateInfo{}
            .setDepthClampEnable(false)
            .setRasterizerDiscardEnable(false)
            .setPolygonMode(vk::PolygonMode::eFill)
            .setLineWidth(1.0f)
            .setCullMode(vk::CullModeFlagBits::eBack)
            .setFrontFace(vk::FrontFace::eCounterClockwise);

        auto multi_sampling = vk::PipelineMultisampleStateCreateInfo{};

        auto colour_blend_attachment = vk::PipelineColorBlendAttachmentState{}
            .setColorWriteMask(vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA)
            .setBlendEnable(false);
        
        auto colour_blend = vk::PipelineColorBlendStateCreateInfo{}
            .setLogicOpEnable(false)
            .setAttachmentCount(1)
            .setPAttachments(&colour_blend_attachment);

        pipeline_layout = PipelineLayoutBuilder{}
            .set_layouts({*descriptor_set_layout})
            .build(device);

        auto colour_attachment = vk::AttachmentDescription{}
            .setFormat(image_format)
            .setSamples(vk::SampleCountFlagBits::e1)
            .setLoadOp(vk::AttachmentLoadOp::eClear)
            .setStoreOp(vk::AttachmentStoreOp::eStore)
            .setStencilLoadOp(vk::AttachmentLoadOp::eDontCare)
            .setStencilStoreOp(vk::AttachmentStoreOp::eDontCare)
            .setInitialLayout(vk::ImageLayout::eUndefined)
            .setFinalLayout(vk::ImageLayout::ePresentSrcKHR);

        auto colour_attachment_ref = vk::AttachmentReference{}
            .setAttachment(0)
            .setLayout(vk::ImageLayout::eColorAttachmentOptimal);
        
        auto subpass = vk::SubpassDescription{}
            .setPipelineBindPoint(vk::PipelineBindPoint::eGraphics)
            .setColorAttachmentCount(1)
            .setPColorAttachments(&colour_attachment_ref);

        render_pass = RenderPassBuilder{}
            .attachments({colour_attachment})
            .subpasses({subpass})
            .dependencies({
                vk::SubpassDependency{}
                    .setSrcSubpass(VK_SUBPASS_EXTERNAL)
                    .setSrcStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput)
                    .setSrcAccessMask({})
                    .setDstSubpass(0)
                    .setDstStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput)
                    .setDstAccessMask(vk::AccessFlagBits::eColorAttachmentWrite)
            })
            .build(device);

        graphics_pipeline = device->createGraphicsPipelineUnique(
            vk::PipelineCache{},
            vk::GraphicsPipelineCreateInfo{}
                .setStageCount(shader_stages.size())
                .setPStages(shader_stages.data())
                .setPVertexInputState(&vertex_input_create_info)
                .setPInputAssemblyState(&assembly_create_info)
                .setPViewportState(&viewport_state)
                .setPRasterizationState(&rasteriser)
                .setPMultisampleState(&multi_sampling)
                .setPColorBlendState(&colour_blend)
                .setLayout(*pipeline_layout)
                .setRenderPass(*render_pass)
                .setSubpass(0)
        ).value;

        for (std::size_t i = 0; i < image_views.size(); i++) {
            framebuffers.push_back(device->createFramebufferUnique(
                vk::FramebufferCreateInfo{}
                    .setRenderPass(*render_pass)
                    .setAttachmentCount(1)
                    .setPAttachments(&image_views[i].get())
                    .setWidth(extent.width)
                    .setHeight(extent.height)
                    .setLayers(1)
            ));
        }

        if (!command_pool) {
            command_pool = device->createCommandPoolUnique(
                vk::CommandPoolCreateInfo{}
                    .setQueueFamilyIndex(queue_family_index)
            );
        }

        if (!vertex_buffer) {
            vertex_buffer = make_buffer(
                vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eTransferDst,
                vertex_buffer_size()
            );
        }

        if (!vertex_buffer_memory) {
            vertex_buffer_memory = make_buffer_memory(
                *vertex_buffer,
                vk::MemoryPropertyFlagBits::eDeviceLocal
            );
            copy_data_to_vertex_buffer(kVertices);
        }

        if (!index_buffer) {
            index_buffer = make_buffer(
                vk::BufferUsageFlagBits::eIndexBuffer | vk::BufferUsageFlagBits::eTransferDst,
                index_buffer_size()
            );
        }

        if (!index_buffer_memory) {
            index_buffer_memory = make_buffer_memory(
                *index_buffer,
                vk::MemoryPropertyFlagBits::eDeviceLocal
            );
            copy_data_to_index_buffer(kIndices);
        }

        if (!texture_image || !texture_image_memory) {
            auto texture = StbImage::from_file("textures/texture.jpg");
            texture_image = make_image(
                texture.extent(),
                vk::Format::eR8G8B8A8Srgb,
                vk::ImageTiling::eOptimal,
                vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled
            );

            texture_image_memory = make_image_memory(
                *texture_image,
                vk::MemoryPropertyFlagBits::eDeviceLocal
            );

            copy_stbimage_to_image(*texture_image, texture);
            transition_image_layout(
                *texture_image,
                vk::Format::eR8G8B8A8Srgb,
                vk::ImageLayout::eTransferDstOptimal,
                vk::ImageLayout::eShaderReadOnlyOptimal
            );
        }

        auto pool_size = vk::DescriptorPoolSize{}
            .setDescriptorCount(image_count);

        descriptor_pool = device->createDescriptorPoolUnique(
            vk::DescriptorPoolCreateInfo{}
                .setPoolSizeCount(1)
                .setPPoolSizes(&pool_size)
                .setMaxSets(image_count)
        );

        std::vector<vk::DescriptorSetLayout> set_layouts(image_count, *descriptor_set_layout);
        descriptor_sets = device->allocateDescriptorSetsUnique(
            vk::DescriptorSetAllocateInfo{}
                .setDescriptorPool(*descriptor_pool)
                .setDescriptorSetCount(image_count)
                .setPSetLayouts(set_layouts.data())
        );

        for (size_t i = 0; i < image_count; i++) {
            uniform_buffers.push_back(
                make_buffer(
                    vk::BufferUsageFlagBits::eUniformBuffer,
                    sizeof(Mvp)
                )
            );
        }

        for (size_t i = 0; i < image_count; i++) {
            uniform_buffers_memory.push_back(
                make_buffer_memory(
                    *uniform_buffers[i],
                    vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent
                )
            );
        }

        for (size_t i = 0; i < image_count; i++) {
            auto buffer_info = vk::DescriptorBufferInfo{}
                .setBuffer(*uniform_buffers[i])
                .setOffset(0)
                .setRange(sizeof(Mvp));

            device->updateDescriptorSets(
                vk::WriteDescriptorSet{}
                    .setDstSet(*descriptor_sets[i])
                    .setDstBinding(0)
                    .setDstArrayElement(0)
                    .setDescriptorType(vk::DescriptorType::eUniformBuffer)
                    .setDescriptorCount(1)
                    .setPBufferInfo(&buffer_info),
                {}
            );
        }


        command_buffers = device->allocateCommandBuffersUnique(
            vk::CommandBufferAllocateInfo{}
                .setCommandPool(*command_pool)
                .setLevel(vk::CommandBufferLevel::ePrimary)
                .setCommandBufferCount(static_cast<std::uint32_t>(framebuffers.size()))
        );

        for (size_t i = 0; i < command_buffers.size(); i++) {
            command_buffers[i]->begin(
                vk::CommandBufferBeginInfo{}
            );

            vk::ClearValue clear_colour{
                vk::ClearColorValue{std::array{0.0f, 0.0f, 0.0f, 1.0f}}
            };

            command_buffers[i]->beginRenderPass(
                vk::RenderPassBeginInfo{}
                    .setRenderPass(*render_pass)
                    .setFramebuffer(*framebuffers[i])
                    .setRenderArea(vk::Rect2D{
                        vk::Offset2D{0, 0},
                        extent,
                    })
                    .setClearValueCount(1)
                    .setPClearValues(&clear_colour),
                vk::SubpassContents::eInline
            );

            command_buffers[i]->bindPipeline(vk::PipelineBindPoint::eGraphics, *graphics_pipeline);
            command_buffers[i]->bindVertexBuffers(0, *vertex_buffer, {0});
            command_buffers[i]->bindIndexBuffer(*index_buffer, 0, vk::IndexType::eUint16);
            command_buffers[i]->bindDescriptorSets(
                vk::PipelineBindPoint::eGraphics,
                *pipeline_layout,
                0,
                *descriptor_sets[i],
                {}
            );
            command_buffers[i]->drawIndexed(static_cast<uint32_t>(kIndices.size()), 1, 0, 0, 0);
            command_buffers[i]->endRenderPass();
            command_buffers[i]->end();
        }

        for (int i = 0; i < 2; i++) {
            image_available_semaphores.push_back(device->createSemaphoreUnique({}));
            render_finished_semaphores.push_back(device->createSemaphoreUnique({}));
            in_flight_fences.push_back(device->createFenceUnique({vk::FenceCreateFlagBits::eSignaled}));
        }
        for (int i = 0; i < 3; i++) {
            image_fences.push_back(vk::Fence{});
        }
    }

    static VulkanState recreate_swapchain(VulkanState vulkan_state) {
        vulkan_state.device->waitIdle();

        VulkanState new_state;
        new_state.queue_family_index = vulkan_state.queue_family_index;
        new_state.instance = std::move(vulkan_state.instance);
        new_state.surface = std::move(vulkan_state.surface);
        new_state.device = std::move(vulkan_state.device);
        new_state.descriptor_set_layout = std::move(vulkan_state.descriptor_set_layout);
        new_state.command_pool = std::move(vulkan_state.command_pool);

        new_state.vertex_buffer = std::move(vulkan_state.vertex_buffer);
        new_state.vertex_buffer_memory = std::move(vulkan_state.vertex_buffer_memory);

        new_state.index_buffer = std::move(vulkan_state.index_buffer);
        new_state.index_buffer_memory = std::move(vulkan_state.index_buffer_memory);

        new_state.texture_image = std::move(vulkan_state.texture_image);
        new_state.texture_image_memory = std::move(vulkan_state.texture_image_memory);

        // destruct the remaining state
        [](VulkanState s){}(std::move(vulkan_state));

        new_state.setup_swapchain();
        return new_state;
    }
private:
    VulkanState() = default;
};


void do_draw(VulkanState& vulkan_state, int in_flight_index, bool resized) {
    vulkan_state.device->waitForFences(
        *vulkan_state.in_flight_fences[in_flight_index],
        true,
        std::numeric_limits<uint32_t>::max()
    );
    auto r = vulkan_state.device->acquireNextImageKHR(
        *vulkan_state.swapchain,
        std::numeric_limits<std::uint64_t>::max(),
        *vulkan_state.image_available_semaphores[in_flight_index],
        vk::Fence{}
    );
    if (r.result == vk::Result::eErrorOutOfDateKHR) {
        vulkan_state = VulkanState::recreate_swapchain(std::move(vulkan_state));
        return;
    }
    uint32_t image_index = r.value;

    if (vulkan_state.image_fences[image_index]) {
        vulkan_state.device->waitForFences(
            vulkan_state.image_fences[image_index],
            true,
            std::numeric_limits<uint32_t>::max()
        );
    }
    vulkan_state.image_fences[image_index] = *vulkan_state.in_flight_fences[in_flight_index];

    auto queue = vulkan_state.queue();
    vulkan_state.device->resetFences(
        *vulkan_state.in_flight_fences[in_flight_index]
    );

    static SpinAnimation animation{std::chrono::steady_clock::now(), 90.0f};
    vulkan_state.update_uniform_buffer(animation, image_index);

    vk::PipelineStageFlags wait_stages = vk::PipelineStageFlagBits::eColorAttachmentOutput;
    queue.submit(
        vk::SubmitInfo{}
            .setWaitSemaphoreCount(1)
            .setPWaitSemaphores(&*vulkan_state.image_available_semaphores[in_flight_index])
            .setPWaitDstStageMask(&wait_stages)
            .setCommandBufferCount(1)
            .setPCommandBuffers(&vulkan_state.command_buffers[image_index].get())
            .setSignalSemaphoreCount(1)
            .setPSignalSemaphores(&*vulkan_state.render_finished_semaphores[in_flight_index]),
        *vulkan_state.in_flight_fences[in_flight_index]
    );

    auto result = queue.presentKHR(
        &vk::PresentInfoKHR{}
            .setWaitSemaphoreCount(1)
            .setPWaitSemaphores(&*vulkan_state.render_finished_semaphores[in_flight_index])
            .setSwapchainCount(1)
            .setPSwapchains(&*vulkan_state.swapchain)
            .setPImageIndices(&image_index)
    );

    if (result == vk::Result::eErrorOutOfDateKHR || result == vk::Result::eSuboptimalKHR || resized) {
        vulkan_state = VulkanState::recreate_swapchain(std::move(vulkan_state));
    }
}

int inner_main() {
    Ctx ctx {
        Serial {
            [](auto& ctx, const auto& event) {std::cout << "Got an event with address: " << &event << std::endl;},
            // The MustHandle wrapper ensures that every event is handled by at least one handler within MustHandle.
            // If we didn't do this the log handler above might hide an error where an event isn't being handled.
            MustHandle { Serial {
                [](auto& ctx, int i){std::cout << ctx.handle_request(i).value() << std::endl;},
                [](auto& ctx, const char* i){std::cout << "c string " << i << std::endl;},
                [](auto& ctx, std::string i){std::cout << "c++ string " << i << std::endl;},

                // Because this handler only wants a const MyEvent& and it happens before
                // the owning handler this will compile.
                [](auto& ctx, const MyEvent& e){std::cout << "Inspecting MyEvent" << std::endl;},
                // Because this is the only handler that wants ownership of MyEvent, MyEvent does
                // not have to be copy constructable. It can be moved into the handler
                [](auto& ctx, MyEvent e){std::cout << "Moved from MyEvent" << std::endl;},

                // Compiler error because MyEvent can't be copied but 2 handlers want ownership
                // [](auto& ctx, MyEvent e){std::cout << "MyEvent is already taken" << std::endl;},

                Buffered {
                    Serial {
                        // [](auto& ctx, const MyEvent& e){std::cout << "Buffered can't move from MyEvent because it has already been moved from" << std::endl;},
                        // Also a compiler error, even though this handler doesn't want ownership of MyEvent
                        // it is inside a Buffered wrapper and Buffered always needs ownership to
                        // make sure the event stays alive long enough. Also a compiler error because the owning
                        // handler for MyEvent comes before this handler, so the MyEvent object
                        // will already have been moved out of.

                        // Buffered will have made a copy of the string event and move it into the Serial.
                        // Because this is the last handler in the Serial that string will be moved into this handler.
                        [](auto& ctx, std::string i){std::cout << "A c++ string " << i << std::endl;},
                    }
                },
                Buffered {
                    [](auto& ctx, std::string i){std::cout << "B c++ string " << i << std::endl;},
                },
            }},
        },


        First {
            [](auto& ctx, int j){return ctx.handle_request("hello");},
            [](auto& ctx, std::unique_ptr<int> i) {return *i;},
            [](auto& ctx, std::unique_ptr<int> i) {return *i;},
            // The first handler returns void, it doesn't need to know the correct type to return.
            [](auto& ctx, const char* c_str){return;},
            // The next handler returns an empty optional, this means it wasn't able
            // to try to answer the request, maybe a setting wasn't been provided or a dll wasn't found. 
            [](auto& ctx, const char* c_str){return std::optional<int>{};},
            // Because the previous handler wasn't able to answer, this handler is called
            [](auto& ctx, const char* c_str){return std::optional<int>(2);},
            // Because the previous handler was able to answer, this handler is not called, but 
            // it still has to return the same type.
            [](auto& ctx, const char* c_str){return std::optional<int>(4);},

            // Can't have a handler return void after another handler has returned non void to 
            // avoid void handlers being shadowed since the only reason to call a void handler would
            // be to produce side effects.
            // [](auto& ctx, const char* c_str){return;},

            // This is ok even though MyRequest is not copyable. This is because
            // the first handler doesn't return a std::optional, this indicates to
            // the compiler that the remaining request handlers never need to be called
            // so MyRequest can be forwarded into the first handler and the second handler
            // is ignored.
            [](auto& ctx, MyRequest req){return "The handler that gets called";},
            // This handler is never called because the handler above doesn't return an optional
            [](auto& ctx, MyRequest req){return "Doesn't get called because something else has non optionally answered MyRequest";},
        }
    };
    

    std::cout << ctx.handle_request(5).value() << std::endl;
    std::cout << ctx.handle_request(MyRequest{}) << std::endl;
    ctx.handle_request(std::make_unique<int>(2));

    ctx.handle_event(MyEvent{});
    ctx.handle_event(std::string("hello"));
    ctx.handle_event(2);

    const int width = 1800;
    const int height = 1000;

    Window window(
        "SDL2 Window",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        width, height,
        SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE
    );

    auto vulkan_state = VulkanState::for_window(window);

    SDL_Event event;
    int in_flight_index = 0;
    bool resized = false;
    bool minimised = false;
    while (true) {
        while (SDL_PollEvent( &event ) != 0) {
            if( event.type == SDL_QUIT ) {
                vulkan_state.device->waitIdle();
                window.close();
                return 0;
            } else if (event.type == SDL_WINDOWEVENT) {
                if (event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
                    resized = true;
                } else if (event.window.event == SDL_WINDOWEVENT_RESIZED) {
                    resized = true;
                } else if (event.window.event == SDL_WINDOWEVENT_MINIMIZED) {
                    minimised = true;
                } else if (event.window.event == SDL_WINDOWEVENT_MAXIMIZED) {
                    minimised = false;
                } else if (event.window.event == SDL_WINDOWEVENT_RESTORED) {
                    minimised = false;
                } else if (event.window.event == SDL_WINDOWEVENT_HIDDEN) {
                    minimised = true;
                } else if (event.window.event == SDL_WINDOWEVENT_SHOWN) {
                    minimised = false;
                } else if (event.window.event == SDL_WINDOWEVENT_EXPOSED) {
                    minimised = false;
                } 
            }
        }
        if (minimised) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        } else {
            do_draw(vulkan_state, in_flight_index, resized);
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
            resized = false;
            in_flight_index = (in_flight_index + 1)%2;
        }
    }
    return 0;
}

int main() {
    try {
        if(SDL_Init(SDL_INIT_VIDEO) < 0) {
            std::cout << "Failed to initialize the SDL2 library\n";
            return -1;
        }
        int r = inner_main();
        SDL_Quit();
        return r;
    } catch (const std::exception& e) {
        std::cout << "Main threw: " << e.what() << std::endl;
        SDL_Quit();
        throw std::current_exception();
    } catch (...) {
        SDL_Quit();
        throw std::current_exception();
    }
}
