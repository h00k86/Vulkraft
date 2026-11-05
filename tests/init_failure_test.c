/* Real orchestration and shutdown, deterministic backend-stage failures. */
#include "../src/render/init_vulkan.c"
static int allocation_number, allocation_failure;
static void *test_calloc(size_t n, size_t size) {
  if (++allocation_number == allocation_failure) return NULL;
  return calloc(n, size);
}
#define calloc test_calloc
#include "../src/render/gfx.c"
#undef calloc
#include <assert.h>
#include <string.h>
static int step, fail_at, live[32];
static bool lost_device;
static bool acquire(int slot) { assert(!live[slot]); live[slot]=1; return ++step != fail_at; }
static void release(int slot) { live[slot]=0; }
#define HANDLE(type, n) ((type)(uintptr_t)(n))
PlatformWindow *create_window(APP *a) { if (!acquire(0)) { release(0); return NULL; } return HANDLE(PlatformWindow *,1); }
void destroy_window(PlatformWindow *w) { if (w) release(0); }
VkInstance create_instance(void) { if (!acquire(1)) { release(1); return VK_NULL_HANDLE; } return HANDLE(VkInstance,1); }
void destroy_instance(VkInstance i) { assert(i); release(1); }
VkSurfaceKHR create_surface(APP *a) { if (!acquire(2)) { release(2); return VK_NULL_HANDLE; } return HANDLE(VkSurfaceKHR,1); }
void destroy_surface(APP *a) { release(2); }
VkPhysicalDevice select_device(VkInstance i, VkSurfaceKHR s) { return ++step == fail_at ? VK_NULL_HANDLE : HANDLE(VkPhysicalDevice,1); }
VkDevice create_logical_device(APP *a) { if (!acquire(3)) { release(3); return VK_NULL_HANDLE; } return HANDLE(VkDevice,1); }
void destroy_logical_device(APP *a) { release(3); }
VkResult vkDeviceWaitIdle(VkDevice d) { assert(d); return lost_device ? VK_ERROR_DEVICE_LOST : VK_SUCCESS; }
Swapchain create_swapchain(APP *a) {
  if (!acquire(4)) { release(4); return (Swapchain){0}; }
  return (Swapchain){.handle=HANDLE(VkSwapchainKHR,1)};
}
bool create_image_views(APP *a) { return acquire(5); }
void destroy_swapchain(APP *a) { release(4); release(5); }
#define STAGE(create, destroy, slot) \
  bool create(APP *a) { return acquire(slot); } \
  void destroy(APP *a) { release(slot); }
STAGE(create_renderpass,destroy_renderpass,6)
bool create_descriptor_set_layout(APP *a) { return acquire(7); }
bool create_descriptor_pool(APP *a) { return acquire(8); }
bool create_hud_descriptor_set(APP *a) { return acquire(9); }
void destroy_descriptors(APP *a) { release(7); release(8); release(9); }
STAGE(create_pipeline,destroy_pipeline,10)
STAGE(create_command_buffers,destroy_command_buffers,11)
STAGE(create_sync_objects,destroy_sync_objects,12)
STAGE(create_framebuffers,destroy_framebuffers,13)
bool create_ubo_camera(APP *a, UniformBuffer *u) { return acquire(14 + (int)(u-a->renderer.gpu->camera_ubo)); }
void destroy_ubo_camera(APP *a, UniformBuffer *u) { release(14 + (int)(u-a->renderer.gpu->camera_ubo)); }
bool create_vertex_buffer_ext(APP *a, VertexBuffer *v, uint32_t n, const char *name) { return acquire(18); }
void destroy_vertex_buffer_ext(APP *a, VertexBuffer *v) { release(18); }
bool app_resource_path(const APP *app, AppResource resource, char *out, size_t cap) {
  snprintf(out, cap, "%s", "font.png"); return ++step != fail_at;
}
void load_texture(APP *a, const char *p, Texture *t) {
  if (!acquire(19)) { release(19); return; }
  t->view=HANDLE(VkImageView,1); t->sampler=HANDLE(VkSampler,1);
}
void destroy_texture(APP *a, Texture *t) { release(19); }
void texture_manager_shutdown(APP *a) {}
void text_renderer_shutdown(void) {}
void frame_dump_shutdown(APP *a) {}
int main(void) {
  APP a = {0};
  for (allocation_failure=1; allocation_failure<=2; ++allocation_failure) {
    allocation_number=0;
    assert(!gfx_init(&a) && a.error.code == APP_ERROR_MEMORY);
    assert(!a.context && !a.renderer.gpu && !a.window && !step);
    gfx_shutdown(&a);
  }
  allocation_failure=0;
  fail_at = -1;
  assert(gfx_init(&a)); int total = step;
  assert(!gfx_init(&a) && a.error.code == APP_ERROR_STATE);
  gfx_shutdown(&a); gfx_shutdown(&a);
  for (int f=1; f<=total; ++f) {
    step=0; fail_at=f;
    assert(!gfx_init(&a)); assert(a.error.code && a.error.stage);
    assert(!a.context && !a.renderer.gpu && !a.window);
    for (int i=0; i<32; ++i) assert(!live[i]);
    gfx_shutdown(&a);
    step=0; fail_at=-1;
    assert(gfx_init(&a) && a.error.code == APP_ERROR_NONE);
    gfx_shutdown(&a);
    for (int i=0; i<32; ++i) assert(!live[i]);
  }
  step=0; fail_at=-1; assert(gfx_init(&a));
  a.error=(AppError){APP_ERROR_GRAPHICS,"runtime submit failure"};
  lost_device=true;
  gfx_shutdown(&a); gfx_shutdown(&a);
  assert(a.error.code==APP_ERROR_GRAPHICS && !strcmp(a.error.stage,"runtime submit failure"));
  assert(!a.window && !a.context && !a.renderer.gpu);
  for (int i=0;i<32;++i) assert(!live[i]);
  assert(!gfx_init(NULL)); gfx_shutdown(NULL);
  printf("Initialization rollback/retry/double shutdown: PASS (%d failure stages)\n", total);
}
