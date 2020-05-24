#include <iostream>
#include <optional>
#include <string>
#include <exception>
#include <memory>
#include <fstream>
#include <array>
#include <limits>
#include <cstddef>

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
#include "glm/glm.hpp"

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
        request_handler(std::move(request_handler)){}

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
    RequestHandlerT request_handler;
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

const std::vector<Vertex> kVertices = {
    {{0.0f, -0.5f}, {1.0f, 0.0f, 0.0f}},
    {{0.5f, 0.5f}, {0.0f, 1.0f, 0.0f}},
    {{-0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}}
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
    vk::UniquePipelineLayout pipeline_layout;
    vk::UniqueRenderPass render_pass;
    vk::UniquePipeline graphics_pipeline;
    std::vector<vk::UniqueFramebuffer> framebuffers;
    vk::UniqueCommandPool command_pool;
    vk::UniqueBuffer vertex_buffer;
    vk::UniqueDeviceMemory vertex_buffer_memory;
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

    void copy_data_to_vertex_buffer(const std::vector<Vertex>& vertices) {
        if (sizeof(vertices[0])*vertices.size() > vertex_buffer_size()) {
            throw std::runtime_error("Failed to copy vertices too much data to copy");
        }

        void* data = device->mapMemory(*vertex_buffer_memory, 0, vertex_buffer_size(), vk::MemoryMapFlags{});
        memcpy(data, vertices.data(), vertices.size() * sizeof(vertices[0]));
        device->unmapMemory(*vertex_buffer_memory);
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
            .setFrontFace(vk::FrontFace::eClockwise);

        auto multi_sampling = vk::PipelineMultisampleStateCreateInfo{};

        auto colour_blend_attachment = vk::PipelineColorBlendAttachmentState{}
            .setColorWriteMask(vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA)
            .setBlendEnable(false);
        
        auto colour_blend = vk::PipelineColorBlendStateCreateInfo{}
            .setLogicOpEnable(false)
            .setAttachmentCount(1)
            .setPAttachments(&colour_blend_attachment);

        pipeline_layout = PipelineLayoutBuilder{}.build(device);

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
            vertex_buffer = device->createBufferUnique(
                vk::BufferCreateInfo{}
                    .setSize(vertex_buffer_size())
                    .setUsage(vk::BufferUsageFlagBits::eVertexBuffer)
                    .setSharingMode(vk::SharingMode::eExclusive)
            );
        }

        const auto mem_requirements = device->getBufferMemoryRequirements(*vertex_buffer);
        vertex_buffer_memory = device->allocateMemoryUnique(
            vk::MemoryAllocateInfo {
                mem_requirements.size,
                find_memory_type(
                    mem_requirements.memoryTypeBits,
                    vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent
                )
            }
        );

        device->bindBufferMemory(*vertex_buffer, *vertex_buffer_memory, 0);
        copy_data_to_vertex_buffer(kVertices);

        command_buffers = device->allocateCommandBuffersUnique(
            vk::CommandBufferAllocateInfo{}
                .setCommandPool(*command_pool)
                .setLevel(vk::CommandBufferLevel::ePrimary)
                .setCommandBufferCount(static_cast<std::uint32_t>(framebuffers.size()))
        );

        for (std::size_t i = 0; i < command_buffers.size(); i++) {
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
            command_buffers[i]->draw(static_cast<uint32_t>(kVertices.size()), 1, 0, 0);
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
        new_state.command_pool = std::move(vulkan_state.command_pool);
        new_state.vertex_buffer = std::move(vulkan_state.vertex_buffer);

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

    auto queue = vulkan_state.device->getQueue(vulkan_state.queue_family_index, 0);
    vulkan_state.device->resetFences(
        *vulkan_state.in_flight_fences[in_flight_index]
    );

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

            // The first handler returns void, it doesn't need to know the correct type to return.
            [](auto& ctx, const char* c_str){return;},
            // The next handler returns an empty optional, this means it wasn't able
            // to answer the request, maybe a setting wasn't been provided or a dll wasn't found. 
            [](auto& ctx, const char* c_str){return std::optional<int>{};},
            // Because the previous handler wasn't able to answer, this handler is called
            [](auto& ctx, const char* c_str){return std::optional<int>(2);},
            // Because the previous handler was able to answer, this handler is not called, but 
            // it still has to return the same type or void.
            [](auto& ctx, const char* c_str){return std::optional<int>(4);},
            [](auto& ctx, const char* c_str){return;},

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
