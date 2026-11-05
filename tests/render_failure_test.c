/* Real draw/command flow; GPU calls fail at each fallible step. */
#include "../src/render/vulkan_command.c"
#include "../src/render/vulkan_draw.c"
#include <assert.h>
#include <stdio.h>
#define H(type,n) ((type)(uintptr_t)(n))
float g_virtualWidth=800, g_virtualHeight=600;
static const char *fail_stage;
static bool tripped, upload=true, recreated_error, graphics_ended;
static VkResult acquire_result, present_result;
static int api_calls, recreations, graphics_submits, presents, graphics_resets, begin_frames;
static VkResult call(const char *stage) {
  assert(!tripped); ++api_calls;
  if (fail_stage && !strcmp(stage,fail_stage)) { tripped=true; return VK_ERROR_DEVICE_LOST; }
  return VK_SUCCESS;
}
VkResult vkWaitForFences(VkDevice d,uint32_t n,const VkFence *f,VkBool32 all,uint64_t timeout) {
  return call(*f==H(VkFence,1)?"frame fence wait":*f==H(VkFence,2)?"orphaned upload wait":"image fence wait");
}
VkResult vkResetCommandBuffer(VkCommandBuffer c,VkCommandBufferResetFlags flags) {
  if(c==H(VkCommandBuffer,20)) ++graphics_resets;
  return call(c==H(VkCommandBuffer,10)?"upload command reset":"graphics command reset");
}
VkResult vkBeginCommandBuffer(VkCommandBuffer c,const VkCommandBufferBeginInfo *i) {
  return call(c==H(VkCommandBuffer,10)?"upload command begin":"graphics command begin");
}
VkResult vkEndCommandBuffer(VkCommandBuffer c) {
  VkResult r=call(c==H(VkCommandBuffer,10)?"upload command end":"graphics command end");
  if(c==H(VkCommandBuffer,20) && r==VK_SUCCESS) graphics_ended=true;
  return r;
}
VkResult vkResetFences(VkDevice d,uint32_t n,const VkFence *f) {
  if(*f==H(VkFence,1)) assert(graphics_ended);
  return call(*f==H(VkFence,2)?"upload fence reset":"graphics fence reset");
}
VkResult vkQueueSubmit(VkQueue q,uint32_t n,const VkSubmitInfo *i,VkFence f) {
  if(f==H(VkFence,1)) ++graphics_submits;
  return call(f==H(VkFence,2)?"upload queue submit":"graphics queue submit");
}
VkResult vkAcquireNextImageKHR(VkDevice d,VkSwapchainKHR s,uint64_t timeout,VkSemaphore semaphore,VkFence fence,uint32_t *index) {
  *index=0; VkResult r=call("swapchain image acquire"); return r==VK_SUCCESS?acquire_result:r;
}
VkResult vkQueuePresentKHR(VkQueue q,const VkPresentInfoKHR *i) {
  ++presents; VkResult r=call("swapchain present"); return r==VK_SUCCESS?present_result:r;
}
void recreate_swapchain(APP *app) {
  assert(!tripped); ++recreations;
  if(recreated_error) app->error=(AppError){APP_ERROR_GRAPHICS,"test recreation"};
}
void vertex_buffer_begin_frame(VertexBuffer *v,uint32_t f) { assert(!tripped); ++begin_frames; }
bool vertex_buffer_record_upload(VertexBuffer *v,uint32_t f,VkCommandBuffer c,VkBufferMemoryBarrier *b) { assert(!tripped); return upload; }
void frame_dump_collect(APP *a,uint32_t f) { assert(!tripped); }
void frame_dump_record(APP *a,GfxCommandList c,uint32_t f,uint32_t i) { assert(!tripped); }
int text_hud_batch_count(void) { return 0; }
const HUDDrawBatch *text_hud_batch_at(int i) { return NULL; }
void vkCmdPipelineBarrier(VkCommandBuffer c,VkPipelineStageFlags s,VkPipelineStageFlags d,VkDependencyFlags flags,uint32_t nm,const VkMemoryBarrier *m,uint32_t nb,const VkBufferMemoryBarrier *b,uint32_t ni,const VkImageMemoryBarrier *i) { assert(!tripped); }
void vkCmdBeginRenderPass(VkCommandBuffer c,const VkRenderPassBeginInfo *i,VkSubpassContents s) { assert(!tripped); }
void vkCmdEndRenderPass(VkCommandBuffer c) { assert(!tripped); }
void vkCmdSetViewport(VkCommandBuffer c,uint32_t f,uint32_t n,const VkViewport *v) { assert(!tripped); }
void vkCmdSetScissor(VkCommandBuffer c,uint32_t f,uint32_t n,const VkRect2D *r) { assert(!tripped); }
void vkCmdBindPipeline(VkCommandBuffer c,VkPipelineBindPoint p,VkPipeline v) { assert(!tripped); }
void vkCmdBindDescriptorSets(VkCommandBuffer c,VkPipelineBindPoint p,VkPipelineLayout l,uint32_t f,uint32_t n,const VkDescriptorSet *s,uint32_t nd,const uint32_t *o) { assert(!tripped); }
void vkCmdBindVertexBuffers(VkCommandBuffer c,uint32_t f,uint32_t n,const VkBuffer *b,const VkDeviceSize *o) { assert(!tripped); }
void vkCmdDraw(VkCommandBuffer c,uint32_t n,uint32_t i,uint32_t f,uint32_t fi) { assert(!tripped); }
static APP app;
static VulkanContext context;
static RendererGpu gpu;
static CameraData mapped;
static VkCommandBuffer commands[MAX_FRAMES_IN_FLIGHT];
static VkFramebuffer framebuffer[1];
static VkFence images[1];
static VkSemaphore semaphores[1];
static void reset(void) {
  app=(APP){0}; context=(VulkanContext){0}; gpu=(RendererGpu){0};
  app.context=&context; app.renderer.gpu=&gpu;
  context.logical_device=H(VkDevice,1);
  context.render_context.commands=(CommandSystem){.buffers=commands,.count=MAX_FRAMES_IN_FLIGHT};
  commands[0]=H(VkCommandBuffer,20);
  context.render_context.frames[0].in_flight_fence=H(VkFence,1);
  context.render_context.transfers[0]=(FrameTransferState){.completion_fence=H(VkFence,2),.command_buffer=H(VkCommandBuffer,10),.submitted=true};
  context.render_context.swapchain=(Swapchain){.handle=H(VkSwapchainKHR,1),.image_count=1,.framebuffers=framebuffer,.extent={800,600},.viewport_rect={.extent={800,600}}};
  context.render_context.images_in_flight=images; images[0]=H(VkFence,3);
  context.render_context.render_finished_semaphores=semaphores;
  context.render_context.render_finished_semaphore_count=1;
  gpu.camera_ubo[0].mapped=&mapped;
  tripped=recreated_error=graphics_ended=false; fail_stage=NULL; upload=true;
  acquire_result=present_result=VK_SUCCESS;
  api_calls=recreations=graphics_submits=presents=graphics_resets=begin_frames=0;
}
int main(void) {
  const char *stages[]={"frame fence wait","orphaned upload wait","upload command reset","upload command begin","upload command end","upload fence reset","upload queue submit","swapchain image acquire","image fence wait","graphics command reset","graphics command begin","graphics command end","graphics fence reset","graphics queue submit","swapchain present"};
  for(size_t i=0;i<sizeof(stages)/sizeof(stages[0]);++i) {
    reset(); fail_stage=stages[i];
    assert(!(wait_for_current_frame(&app) && draw_frame(&app)));
    assert(tripped && app.error.code==APP_ERROR_GRAPHICS && !strcmp(app.error.stage,stages[i]));
    assert(context.render_context.current_frame==0);
    int calls=api_calls;
    assert(!wait_for_current_frame(&app) && !draw_frame(&app) && api_calls==calls);
    assert(!submit_pending_uploads(&app,0) && !record_command_buffer(&app,0,0) && api_calls==calls);
    assert(!vulkan_check(&app,VK_ERROR_OUT_OF_HOST_MEMORY,"later error"));
    assert(!strcmp(app.error.stage,stages[i]));
  }
  reset(); assert(wait_for_current_frame(&app) && draw_frame(&app));
  assert(graphics_submits==1 && presents==1 && context.render_context.current_frame==1);
  assert(context.render_context.transfers[0].covered_by_render_fence && images[0]==H(VkFence,1));
  reset(); upload=false;
  assert(wait_for_current_frame(&app) && draw_frame(&app));
  assert(!context.render_context.transfers[0].submitted && graphics_submits==1);
  reset(); acquire_result=VK_ERROR_OUT_OF_DATE_KHR;
  assert(wait_for_current_frame(&app) && draw_frame(&app));
  assert(recreations==1 && !graphics_submits && !graphics_resets && !presents);
  assert(context.render_context.transfers[0].submitted && !context.render_context.transfers[0].covered_by_render_fence);
  reset(); acquire_result=VK_SUBOPTIMAL_KHR;
  assert(wait_for_current_frame(&app) && draw_frame(&app)); assert(recreations==1 && presents==1);
  reset(); present_result=VK_SUBOPTIMAL_KHR;
  assert(wait_for_current_frame(&app) && draw_frame(&app)); assert(recreations==1);
  reset(); present_result=VK_ERROR_OUT_OF_DATE_KHR;
  assert(wait_for_current_frame(&app) && draw_frame(&app)); assert(recreations==1);
  reset(); app.framebuffer_resized=true; fail_stage="swapchain present";
  assert(wait_for_current_frame(&app) && !draw_frame(&app)); assert(!recreations);
  reset(); acquire_result=VK_ERROR_OUT_OF_DATE_KHR; recreated_error=true;
  assert(wait_for_current_frame(&app) && !draw_frame(&app)); assert(!strcmp(app.error.stage,"test recreation"));
  reset(); upload=false; gpu.hud_buffer.upload_requested[0]=true;
  assert(wait_for_current_frame(&app) && !draw_frame(&app)); assert(app.error.code==APP_ERROR_STATE);
  reset(); context.render_context.current_frame=MAX_FRAMES_IN_FLIGHT;
  assert(!draw_frame(&app) && !api_calls && app.error.code==APP_ERROR_STATE);
  puts("Render failure propagation: PASS (15 injected stages, fail-stop reentry, recovery results)");
}
