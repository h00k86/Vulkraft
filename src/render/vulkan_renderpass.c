#include "render/vulkan_renderpass.h"
#include "render/gfx_vulkan.h"
#include "app/application.h"
#include "log/log.h"
#include <stdlib.h>

void create_renderpass(APP *app) {
  VkDevice device = VK_CTX(app)->logical_device;

  VkAttachmentDescription colorAtt = {0};

  colorAtt.format = VK_CTX(app)->render_context.swapchain.format;

  colorAtt.samples = VK_SAMPLE_COUNT_1_BIT;

  colorAtt.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;

  colorAtt.storeOp = VK_ATTACHMENT_STORE_OP_STORE;

  colorAtt.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;

  colorAtt.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;

  colorAtt.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

  colorAtt.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

  VkAttachmentReference colorRef = {0,
                                    VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};

  VkSubpassDescription subpass = {0};

  subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;

  subpass.colorAttachmentCount = 1;

  subpass.pColorAttachments = &colorRef;

  subpass.pDepthStencilAttachment = NULL;

  VkAttachmentDescription attachments[1] = {colorAtt};

  // DUE dipendenze, non una — quella USCENTE (subpass 0 -> esterno, per
  // consegnare l'immagine pronta alla transizione di layout verso
  // PRESENT_SRC_KHR) E quella ENTRANTE (esterno -> subpass 0, per
  // aspettare che l'immagine appena acquisita dalla swapchain sia pronta
  // prima di scriverci sopra). Prima ne avevo solo una: Vulkan fornisce
  // ENTRAMBE gratis di default SOLO se non ne specifichi NESSUNA — nel
  // momento in cui ne dichiari anche una sola esplicita, quella
  // implicita sparisce del tutto, non solo quella che hai sostituito. La
  // validazione di sincronizzazione lo ha confermato: "WRITE_AFTER_READ...
  // scritto da vkCmdBeginRenderPass, letto in precedenza da
  // vkAcquireNextImageKHR".
  VkSubpassDependency dependencies[2] = {0};

  dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
  dependencies[0].dstSubpass = 0;
  dependencies[0].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  dependencies[0].srcAccessMask = 0;
  dependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  dependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

  // Uscita (subpass 0 -> esterno): qui, a differenza del renderpass
  // OFFSCREEN sotto, l'immagine finisce in PRESENT_SRC_KHR e va dritta a
  // vkQueuePresentKHR — NON viene mai riletta da uno shader. La versione
  // precedente di questa dependency diceva "per la lettura dello shader
  // nel secondo passaggio" ed usava dstStageMask=FRAGMENT_SHADER_BIT:
  // sembra copiata dal renderpass offscreen (dove quello è corretto,
  // vedi create_offscreen_renderpass) senza essere adattata qui. Non
  // pericolosa di per sé (una dependency più restrittiva del necessario
  // non causa corruzione, solo un'attesa inutile a uno stage — il
  // fragment shader — che la presentazione non raggiunge mai in questo
  // modo), ma logicamente sbagliata: qui basta assicurarsi che la
  // scrittura del colore sia finita prima della transizione di layout
  // implicita a PRESENT_SRC_KHR, non prima di una lettura da shader.
  dependencies[1].srcSubpass = 0;
  dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
  dependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  dependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
  dependencies[1].dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
  dependencies[1].dstAccessMask = 0;

  VkRenderPassCreateInfo info = {0};

  info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;

  info.attachmentCount = 1;

  info.pAttachments = attachments;

  info.subpassCount = 1;

  info.pSubpasses = &subpass;

  info.dependencyCount = 2;

  info.pDependencies = dependencies;

  if (vkCreateRenderPass(device, &info, NULL,
                         &VK_CTX(app)->render_context.renderpass) !=
      VK_SUCCESS) {
    LOG_ERROR("renderpass creation failed");
    exit(1);
  }

  LOG_INFO("renderpass created");
}

void destroy_renderpass(APP *app) {
  if (VK_CTX(app)->render_context.renderpass != VK_NULL_HANDLE) {
    vkDestroyRenderPass(VK_CTX(app)->logical_device,
                        VK_CTX(app)->render_context.renderpass, NULL);

    VK_CTX(app)->render_context.renderpass = VK_NULL_HANDLE;
  }
}
