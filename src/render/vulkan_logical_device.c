#include "render/vulkan_logical_device.h"
#include "render/gfx_vulkan.h"
#include "log/log.h"
#include "render/vulkan_physical_device.h"

#ifdef DEBUG
static void print_queue_properties(APP *app) {

  uint32_t queueCount = 0;
  vkGetPhysicalDeviceQueueFamilyProperties(VK_CTX(app)->physical_device,
                                           &queueCount, NULL);
  LOG_INFO("numero di queue per il device: %d", queueCount);

  VkQueueFamilyProperties queueProps[queueCount];
  vkGetPhysicalDeviceQueueFamilyProperties(VK_CTX(app)->physical_device,
                                           &queueCount, queueProps);
  for (uint32_t i = 0; i < queueCount; i++) {
    LOG_INFO("queue %d", i);
    if (queueProps[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
      LOG_INFO("graphics queue supported");
    if (queueProps[i].queueFlags & VK_QUEUE_COMPUTE_BIT)
      LOG_INFO("compute queue supported");

    if (queueProps[i].queueFlags & VK_QUEUE_SPARSE_BINDING_BIT)
      LOG_INFO("sparse binding queue supported");

    if (queueProps[i].queueFlags & VK_QUEUE_VIDEO_DECODE_BIT_KHR)
      LOG_INFO(" video decode supported");

    if (queueProps[i].queueFlags & VK_QUEUE_OPTICAL_FLOW_BIT_NV)
      LOG_INFO("optical supported");

    if (queueProps[i].queueFlags & VK_QUEUE_DATA_GRAPH_BIT_ARM)
      LOG_INFO("data graph supported");

    if (queueProps[i].queueFlags & VK_QUEUE_VIDEO_ENCODE_BIT_KHR)
      LOG_INFO("video encode supported");

    if (queueProps[i].queueFlags & VK_QUEUE_TRANSFER_BIT)
      LOG_INFO("transfer queue supported");
    if (queueProps[i].queueFlags & VK_QUEUE_PROTECTED_BIT)
      LOG_INFO("protected queue supported");
  }
}
#endif

VkDevice create_logical_device(APP *app) {

#ifdef DEBUG
  print_queue_properties(app);
#endif
  uint32_t familyqueue_index = 0;
  if (!find_graphics_present_queue_family(
          VK_CTX(app)->physical_device, VK_CTX(app)->render_context.surface,
          &familyqueue_index)) {
    LOG_ERROR("No GRAPHICS+PRESENT queue family found!");
    exit(1);
  }

  VkDeviceQueueCreateInfo queueCreateInfo = {0};
  queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
  queueCreateInfo.queueFamilyIndex = familyqueue_index;
  queueCreateInfo.queueCount = 1;
  float queuePriority = 1.0f;
  queueCreateInfo.pQueuePriorities = &queuePriority;

  /* FILTRO ANISOTROPICO (2026-09-26): va CHIESTO alla creazione del device,
   * altrimenti il sampler non puo' usarlo. Prima qui pEnabledFeatures era
   * NULL, quindi nessuna feature era attiva e ogni sampler restava
   * isotropo. Serve alle superfici viste di scorcio -- con una camera
   * inclinata di 40 gradi sono il terreno e i fianchi di ogni muro, cioe'
   * quasi tutto. Si chiede solo se la GPU la espone: dove non c'e', si
   * continua come prima. */
  VkPhysicalDeviceFeatures supported;
  vkGetPhysicalDeviceFeatures(VK_CTX(app)->physical_device, &supported);
  VkPhysicalDeviceFeatures wanted = {0};
  wanted.samplerAnisotropy = supported.samplerAnisotropy;

  const char *devExt[] = {"VK_KHR_swapchain"};
  VkDeviceCreateInfo dInfo = {0};
  dInfo.pEnabledFeatures = &wanted;
  dInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
  dInfo.queueCreateInfoCount = 1;
  dInfo.pQueueCreateInfos = &queueCreateInfo;
  dInfo.enabledExtensionCount = 1;
  dInfo.ppEnabledExtensionNames = devExt;

  VkDevice device;
  VkResult res =
      vkCreateDevice(VK_CTX(app)->physical_device, &dInfo, NULL, &device);

  if (res != VK_SUCCESS) {
    LOG_ERROR("Failed to create logical device!");
    exit(1);
  }

  // salvo indice per la queue
  VK_CTX(app)->family_queue_index = familyqueue_index;

  vkGetDeviceQueue(device, familyqueue_index, 0, &VK_CTX(app)->graphic_queue);
  return device;
}

void destroy_logical_device(APP *app) {

  LOG_INFO("destroing logical device");
  vkDestroyDevice(VK_CTX(app)->logical_device, NULL);
  LOG_INFO("destroied logical device");
}
