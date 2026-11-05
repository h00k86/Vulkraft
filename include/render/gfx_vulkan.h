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
    VkDeviceSize size;
} UniformBuffer;

typedef struct {
    VkSemaphore image_available_semaphore;
    // NIENTE render_finished_semaphore qui: vive per IMMAGINE della
    // swapchain, non per frame in volo. Vedi render_finished_semaphores
    // in RenderContext e il commento in vulkan_sync.c.
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
    // camera3d.md fase 2 (decisione 6: bande nere): rettangolo utile 16:9
    // centrato nella swapchain. Viewport/scissor delle pipeline di mondo e
    // HUD, culling e mapping del mouse lavorano su QUESTO rettangolo; il
    // post-processing copre l'intera swapchain e dipinge di nero il resto.
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

    // Cleared only after the frame-slot render fence has completed, then set
    // by update_vertex_buffer. Performance Lab uses this to count actual
    // uploads rather than infer them from a non-zero draw count.
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
    uint32_t peak_vertex_count;
    uint32_t capacity_growth_count;
    uint32_t storage_generation;
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
