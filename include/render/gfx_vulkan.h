#pragma once
// Private backend header. Must never be included above src/render.
#include "render/gfx.h"
#include "app/application.h"
#include "render/vulkan_texture.h"
#include <vulkan/vulkan.h>
typedef struct {
    VkBuffer buffer;
    VkDeviceMemory memory;
    void* mapped;
} UniformBuffer;

typedef struct {
    VkSemaphore image_available_semaphore;

    VkFence in_flight_fence;
} FrameSyncObjects;

typedef struct {
    VkCommandPool pool;
    VkCommandBuffer *buffers;
    uint32_t count;
} CommandSystem;

typedef struct {
    VkCommandBuffer command_buffer;
    VkFence completion_fence;
    bool submitted;
    bool covered_by_render_fence;
} FrameTransferState;

typedef struct {
    VkPipeline handle;
    VkPipelineLayout layout;
} Pipeline;

typedef struct {
    VkSwapchainKHR handle;
    VkImage* images;
    VkImageView* image_views;
    uint32_t image_count;
    VkFormat format;
    VkPresentModeKHR present_mode;
    bool transfer_src;
    VkExtent2D extent;
    VkFramebuffer *framebuffers;

    /* Canvas viewport/scissor in swapchain pixels; currently the whole surface. */
    VkRect2D viewport_rect;
} Swapchain;

typedef struct VertexBuffer {
    VkBuffer gpu_buffer[MAX_FRAMES_IN_FLIGHT]; // prima uno solo
                                               // condiviso — una copia
                                               // per il frame N poteva
                                               // sovrascrivere dati che
                                               // il disegno del frame
                                               // N-1 stava ancora
                                               // leggendo, confermato
                                               // dalla validazione di
                                               // sincronizzazione
                                               // (WRITE_AFTER_READ)
    VkDeviceMemory gpu_buffer_memory[MAX_FRAMES_IN_FLIGHT];

    VkBuffer staging_buffer[MAX_FRAMES_IN_FLIGHT];
    VkDeviceMemory staging_buffer_memory[MAX_FRAMES_IN_FLIGHT];
    void* mapped[MAX_FRAMES_IN_FLIGHT];

    bool upload_requested[MAX_FRAMES_IN_FLIGHT];
    VkDeviceSize upload_size[MAX_FRAMES_IN_FLIGHT];
    struct {
        VkDeviceSize offset;
        VkDeviceSize size;
    } upload_ranges[MAX_FRAMES_IN_FLIGHT][2];
    uint32_t upload_range_count[MAX_FRAMES_IN_FLIGHT];

    uint32_t vertex_count;
    uint32_t capacity;
    uint32_t maximum_capacity;
    VkDeviceSize size;
    const char *debug_name;
} VertexBuffer;

struct RenderContext {
    VkSurfaceKHR surface;
    Swapchain swapchain;
    VkRenderPass renderpass;
    CommandSystem commands;
    FrameTransferState transfers[MAX_FRAMES_IN_FLIGHT];
    FrameSyncObjects frames[MAX_FRAMES_IN_FLIGHT];
    uint32_t current_frame;
    VkFence *images_in_flight;
    VkSemaphore *render_finished_semaphores;
    uint32_t render_finished_semaphore_count;
};
struct VulkanContext {
    VkInstance instance;
    VkPhysicalDevice physical_device;
    VkDevice logical_device;
    uint32_t family_queue_index;
    VkQueue graphic_queue;
    RenderContext render_context;
};
struct RendererGpu {
    Pipeline pipeline;
    VkDescriptorSetLayout descriptor_set_layout;
    VkDescriptorPool descriptor_pool;
    VkDescriptorSet hud_descriptor_set[MAX_FRAMES_IN_FLIGHT];
    UniformBuffer camera_ubo[MAX_FRAMES_IN_FLIGHT];
    VertexBuffer hud_buffer;
    Texture texture;
};
static inline VulkanContext *gfx_vk_context_empty(void) {
  static VulkanContext empty;
  return &empty;
}
#define VK_CTX(g) (((g) && (g)->context) ? (g)->context : gfx_vk_context_empty())
