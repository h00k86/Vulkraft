#include "render/platform_surface.h"
#include "platform/window.h"
#include "render/vulkan_instance.h"
#include "render/gfx_vulkan.h"
#include "log/log.h"
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

// TODO: bisogna ancora aggiungere i check per le estensioni per vedere
// se sono supportate. Il check sui validation layer (debug) è stato
// aggiunto sotto, in create_instance.
//

VkInstance create_instance() {

  uint32_t extCount = 0;
  const char **exts = platform_vulkan_extensions(&extCount);
  if (!exts || !extCount) return VK_NULL_HANDLE;
  VkApplicationInfo appInfo = {0};
  appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
  appInfo.pApplicationName = "Vulkan app";
  appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
  appInfo.pEngineName = "NoEngine";
  appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
  appInfo.apiVersion = VK_API_VERSION_1_3;

  VkInstanceCreateInfo instInfo = {0};
  instInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
  instInfo.pApplicationInfo = &appInfo;
  instInfo.enabledExtensionCount = extCount;
  instInfo.ppEnabledExtensionNames = exts;

#ifdef DEBUG
  const char *layers[] = {"VK_LAYER_KHRONOS_validation"};

  // Via di fuga per quando serve iterare veloce SENZA la validazione:
  // il layer controlla ogni singola chiamata Vulkan, prezioso quando
  // stai scrivendo codice Vulkan nuovo, ma costa CPU reale — con
  // sistemi che fanno molte chiamate pesanti (il particellare, due
  // vkCmdDraw da 'capacity * 6' vertici ogni frame più un dispatch
  // compute) può bastare da solo a far sembrare "lento" qualcosa che
  // in realtà non lo è affatto in release. NO_VALIDATION=1 tiene tutto
  // il resto di DEBUG (simboli, assert, -O0) ma salta SOLO
  // l'attivazione di questo layer — un modo di testare "si sente
  // fluido?" senza dover ricompilare in release e perdere gdb.
  bool validationRequested = getenv("NO_VALIDATION") == NULL;

  uint32_t availableLayerCount = 0;
  if (vkEnumerateInstanceLayerProperties(&availableLayerCount, NULL) != VK_SUCCESS) return VK_NULL_HANDLE;
  VkLayerProperties *availableLayers =
      malloc(sizeof(VkLayerProperties) * availableLayerCount);
  if (availableLayerCount && (!availableLayers ||
      vkEnumerateInstanceLayerProperties(&availableLayerCount, availableLayers) != VK_SUCCESS)) {
    free(availableLayers); return VK_NULL_HANDLE;
  }

  bool validationAvailable = false;
  for (uint32_t i = 0; i < availableLayerCount; i++) {
    if (strcmp(availableLayers[i].layerName, layers[0]) == 0) {
      validationAvailable = true;
      break;
    }
  }
  free(availableLayers);

  if (validationRequested && validationAvailable) {
    instInfo.enabledLayerCount = 1;
    instInfo.ppEnabledLayerNames = layers;
    LOG_INFO("Vulkan validation attiva: %s", layers[0]);
  } else if (!validationRequested) {
    LOG_INFO("NO_VALIDATION impostata — build DEBUG senza validation "
            "layers (più CPU per il resto del gioco, meno rete di "
            "sicurezza su eventuali errori Vulkan)");
  } else {
    LOG_WARN("VK_LAYER_KHRONOS_validation non disponibile su questa "
             "macchina (Vulkan SDK non installato?) — build DEBUG senza "
             "validation layers");
  }
#endif
  VkInstance instance;

  if (vkCreateInstance(&instInfo, NULL, &instance) != VK_SUCCESS) {

    LOG_ERROR("error creation instance\n");
    return VK_NULL_HANDLE;
  }

  return instance;
}

void destroy_instance(VkInstance instance) {
  LOG_INFO("destroying VkInstance");
  vkDestroyInstance(instance, NULL);
  LOG_INFO("destroied VkInstance");
}
