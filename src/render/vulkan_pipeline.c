#include "render/vulkan_pipeline.h"
#include "render/gfx_vulkan.h"
#include "app/application.h"
#include "platform/paths.h"
#include <stddef.h>

static bool create_pipeline_for_renderpass(APP *app, VkRenderPass renderPass,
                                           VkPipelineLayout sharedLayout,
                                           const char *fragPath,
                                           Pipeline *out) {
  VkDevice device = VK_CTX(app)->logical_device;
  RenderContext *ctx = &VK_CTX(app)->render_context;

  /* Resolve the configured vertex shader again during surface recreation.
   * Both shaders must retain the renderer's vertex/descriptor interface. */
  char vertex_path[APP_PATH_MAX];
  if (!app_resource_path(app, APP_RESOURCE_VERTEX_SHADER, vertex_path, sizeof(vertex_path))) return false;
  VkShaderModule vertModule = load_shader(device, vertex_path);
  if (!vertModule) return false;
  VkShaderModule fragModule = load_shader(device, fragPath);
  if (!fragModule) { vkDestroyShaderModule(device, vertModule, NULL); return false; }

  VkPipelineShaderStageCreateInfo stages[2] = {0};
  stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
  stages[0].module = vertModule;
  stages[0].pName = "main";

  stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
  stages[1].module = fragModule;
  stages[1].pName = "main";

  VkVertexInputBindingDescription binding = {0};
  binding.binding = 0;
  binding.stride = sizeof(Vertex);
  binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

  // Vertex locations match the public shader interface (0, 2, 3, 4).
  VkVertexInputAttributeDescription attributes[4];
  attributes[0] = (VkVertexInputAttributeDescription){0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, pos)};
  attributes[1] = (VkVertexInputAttributeDescription){2, 0, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(Vertex, color)};
  attributes[2] = (VkVertexInputAttributeDescription){3, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(Vertex, uv)};
  attributes[3] = (VkVertexInputAttributeDescription){4, 0, VK_FORMAT_R32_SINT, offsetof(Vertex, textureID)};

  VkPipelineVertexInputStateCreateInfo vi = {0};
  vi.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
  vi.vertexBindingDescriptionCount = 1;
  vi.pVertexBindingDescriptions = &binding;
  vi.vertexAttributeDescriptionCount = 4;
  vi.pVertexAttributeDescriptions = attributes;

  VkPipelineInputAssemblyStateCreateInfo ia = {0};
  ia.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
  ia.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
  ia.primitiveRestartEnable = VK_FALSE;

  // Initial viewport; dynamic viewport/scissor are set for each frame.
  VkViewport viewport = {.x = (float)ctx->swapchain.viewport_rect.offset.x,
                         .y = (float)ctx->swapchain.viewport_rect.offset.y,
                         .width = (float)ctx->swapchain.viewport_rect.extent.width,
                         .height = (float)ctx->swapchain.viewport_rect.extent.height,
                         .minDepth = 0.0f,
                         .maxDepth = 1.0f};

  VkRect2D scissor = ctx->swapchain.viewport_rect;

  VkPipelineViewportStateCreateInfo vp = {0};
  vp.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
  vp.viewportCount = 1;
  vp.pViewports = &viewport;
  vp.scissorCount = 1;
  vp.pScissors = &scissor;

  VkPipelineRasterizationStateCreateInfo rs = {0};
  rs.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
  rs.depthClampEnable = VK_FALSE;
  rs.rasterizerDiscardEnable = VK_FALSE;
  rs.polygonMode = VK_POLYGON_MODE_FILL;
  rs.lineWidth = 1.0f;
  rs.cullMode = VK_CULL_MODE_NONE;
  rs.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
  rs.depthBiasEnable = VK_FALSE;

  VkPipelineMultisampleStateCreateInfo ms = {0};
  ms.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
  ms.sampleShadingEnable = VK_FALSE;
  ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

  VkPipelineColorBlendAttachmentState attach = {0};
  attach.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                          VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
  attach.blendEnable = VK_TRUE;
  attach.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
  attach.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
  attach.colorBlendOp = VK_BLEND_OP_ADD;
  attach.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
  attach.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
  attach.alphaBlendOp = VK_BLEND_OP_ADD;

  VkPipelineColorBlendStateCreateInfo blend = {0};
  blend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
  blend.logicOpEnable = VK_FALSE;
  blend.attachmentCount = 1;
  blend.pAttachments = &attach;

  VkPipelineDepthStencilStateCreateInfo depthStencil = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
      .depthTestEnable = VK_FALSE,
      .depthWriteEnable = VK_FALSE,
      .depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL,
      .depthBoundsTestEnable = VK_FALSE,
      .stencilTestEnable = VK_FALSE,
  };

  /* Viewport and clipping follow the current canvas dimensions. */
  VkDynamicState dynamicStates[] = {VK_DYNAMIC_STATE_SCISSOR, VK_DYNAMIC_STATE_VIEWPORT};
  VkPipelineDynamicStateCreateInfo dynamicState = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
      .dynamicStateCount = 2,
      .pDynamicStates = dynamicStates};

  out->layout = sharedLayout;

  VkGraphicsPipelineCreateInfo gp = {0};
  gp.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
  gp.stageCount = 2;
  gp.pStages = stages;
  gp.pVertexInputState = &vi;
  gp.pInputAssemblyState = &ia;
  gp.pViewportState = &vp;
  gp.pRasterizationState = &rs;
  gp.pMultisampleState = &ms;
  gp.pDepthStencilState = &depthStencil;
  gp.pColorBlendState = &blend;
  gp.pDynamicState = &dynamicState;
  gp.layout = sharedLayout;
  gp.renderPass = renderPass;
  gp.subpass = 0;
  gp.basePipelineHandle = VK_NULL_HANDLE;
  gp.basePipelineIndex = -1;

  VkResult result = vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &gp,
                                              NULL, &out->handle);
  vkDestroyShaderModule(device, vertModule, NULL);
  vkDestroyShaderModule(device, fragModule, NULL);
  if (result != VK_SUCCESS) {
    LOG_ERROR("failed to create graphics pipeline");
    return false;
  }

  return true;
}

bool create_pipeline(APP *app) {
  char fragment_path[APP_PATH_MAX];
  if (!app_resource_path(app, APP_RESOURCE_FRAGMENT_SHADER, fragment_path, sizeof(fragment_path))) return false;
  VkPipelineLayoutCreateInfo layoutInfo = {0};
  layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  layoutInfo.setLayoutCount = 1;
  layoutInfo.pSetLayouts = &app->renderer.gpu->descriptor_set_layout;
  VkPipelineLayout sharedLayout;
  if (vkCreatePipelineLayout(VK_CTX(app)->logical_device, &layoutInfo, NULL, &sharedLayout) != VK_SUCCESS) return false;
  app->renderer.gpu->pipeline.layout = sharedLayout;
  return create_pipeline_for_renderpass(app, VK_CTX(app)->render_context.renderpass, sharedLayout, fragment_path, &app->renderer.gpu->pipeline);
}

void destroy_pipeline(APP *app) {
  VkDevice device = VK_CTX(app)->logical_device;
  if (app->renderer.gpu->pipeline.handle)
    vkDestroyPipeline(device, app->renderer.gpu->pipeline.handle, NULL);
  if (app->renderer.gpu->pipeline.layout)
    vkDestroyPipelineLayout(device, app->renderer.gpu->pipeline.layout, NULL);
  app->renderer.gpu->pipeline = (Pipeline){0};
}
