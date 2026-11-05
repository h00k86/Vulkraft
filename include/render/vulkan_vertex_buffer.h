#pragma once
#include "render/gfx_vulkan.h" /* PORTING E14: tipi del backend */

#include <vulkan/vulkan.h>
#include <stdint.h>
#include <stdbool.h>

typedef struct APP APP;



bool create_vertex_buffer_ext(APP *app, VertexBuffer *vb,
                              uint32_t initial_vertex_capacity,
                              const char *debug_name);
bool create_vertex_buffer_ext_limited(APP *app, VertexBuffer *vb,
                                      uint32_t initial_vertex_capacity,
                                      uint32_t maximum_vertex_capacity,
                                      const char *debug_name);
void update_vertex_buffer(APP *app, VertexBuffer *vb, const Vertex *data,
                          uint32_t count);
bool vertex_buffer_prepare(APP *app, VertexBuffer *vb, uint32_t count);
bool vertex_buffer_stage_range(APP *app, VertexBuffer *vb,
                               const Vertex *data, uint32_t first_vertex,
                               uint32_t count);
void vertex_buffer_begin_frame(VertexBuffer *vb, uint32_t frame);
bool vertex_buffer_record_upload(VertexBuffer *vb, uint32_t frame,
                                 VkCommandBuffer command_buffer,
                                 VkBufferMemoryBarrier *out_barrier);
void destroy_vertex_buffer_ext(APP *app, VertexBuffer *vb);
