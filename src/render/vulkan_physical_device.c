

#include "render/vulkan_physical_device.h"
#include "render/gfx_vulkan.h"

#include "platform/platform_fs.h"
#include <stdio.h>
#include <string.h>

static uint32_t count_device(VkInstance instance) {

  uint32_t count = 0;
  if (vkEnumeratePhysicalDevices(instance, &count, NULL) != VK_SUCCESS) return 0;
  return count;
}

static bool read_hex_id(const char *path, unsigned *out) {
  FILE *f = fopen(path, "r");
  if (!f)
    return false;
  unsigned value = 0;
  bool ok = fscanf(f, "0x%x", &value) == 1;
  fclose(f);
  if (ok)
    *out = value;
  return ok;
}

// Questa GPU ha uno schermo attaccato?
//
// Su un portatile ibrido "muxless" la GPU discreta puo' non avere NESSUN
// connettore video cablato: renderizza soltanto, e ogni frame va copiato
// alla integrata per essere mostrato. Quella copia NON compare nelle query
// di timestamp dell'applicazione — la aggiunge il driver — ma si paga per
// intero ad ogni frame, indipendentemente da cosa c'e' nella scena. E'
// esattamente il sintomo "rallenta anche a livello vuoto".
//
// Misure su una macchina Intel UHD 620 + Radeon 550, dove la discreta ha
// zero connettori (report benchmarks/results/HYBRID_GPU_PRESENT_P3_H):
// stesso lavoro GPU (~2,5 ms) su entrambe, ma frame 5,57 ms sulla discreta
// contro 2,49 ms sull'integrata, di cui 2,53 ms fermi dentro
// vkAcquireNextImageKHR ad aspettare un'immagine libera. Con editor e
// overlay: 12,23 contro 4,59 ms. radeontop vedeva la discreta al 100%
// mentre l'applicazione ne misurava il 45%: il resto era la copia.
//
// Percio' "ha un'uscita collegata" pesa PIU' di "e' discreta". Una scheda
// piu' potente che deve consegnare l'immagine a un'altra parte da un frame
// di svantaggio, e con questo carico non lo recupera: nella stessa prova la
// discreta disegnava in 2,47 ms contro 4,51 ms, e perdeva lo stesso.
//
// Il dato si legge da /sys/class/drm senza estensioni Vulkan ne' dipendenze
// nuove: ogni cardN espone device/vendor e device/device (gli stessi ID che
// Vulkan riporta in VkPhysicalDeviceProperties), e i suoi connettori sono le
// voci "cardN-<nome>" con un file status che dice "connected".
//
// Ritorna 1 = uscita collegata, 0 = nessuna, -1 = non determinabile. Il -1
// e' importante: su un sistema senza sysfs (o non Linux) non si penalizza
// nessuno e resta valido il criterio per tipo di dispositivo.
static int gpu_has_connected_display(uint32_t vendorID, uint32_t deviceID) {

  PlatformDirIter it;
  if (!platform_dir_open(&it, "/sys/class/drm"))
    return -1;

  char cardName[64] = "";
  PlatformDirEntry entry;
  while (platform_dir_next(&it, &entry)) {
    // Solo le schede "cardN", non i connettori "cardN-eDP-1".
    if (strncmp(entry.name, "card", 4) != 0)
      continue;
    const char *suffix = entry.name + 4;
    if (*suffix == '\0')
      continue;
    bool digitsOnly = true;
    for (const char *c = suffix; *c; ++c) {
      if (*c < '0' || *c > '9') {
        digitsOnly = false;
        break;
      }
    }
    if (!digitsOnly)
      continue;

    char path[512];
    unsigned ven = 0, dev = 0;
    snprintf(path, sizeof(path), "/sys/class/drm/%s/device/vendor",
             entry.name);
    if (!read_hex_id(path, &ven))
      continue;
    snprintf(path, sizeof(path), "/sys/class/drm/%s/device/device",
             entry.name);
    if (!read_hex_id(path, &dev))
      continue;
    if (ven != vendorID || dev != deviceID)
      continue;

    // Un nome piu' lungo del buffer non e' una cardN reale: scartare e'
    // piu' onesto che troncare e poi confrontare un prefisso sbagliato.
    const size_t nameLen = strlen(entry.name);
    if (nameLen >= sizeof(cardName))
      continue;
    memcpy(cardName, entry.name, nameLen + 1);
    break;
  }
  platform_dir_close(&it);

  if (cardName[0] == '\0')
    return -1; // nessuna cardN corrisponde a questi ID PCI

  if (!platform_dir_open(&it, "/sys/class/drm"))
    return -1;

  int connected = 0;
  const size_t prefix = strlen(cardName);
  while (platform_dir_next(&it, &entry)) {
    if (strncmp(entry.name, cardName, prefix) != 0)
      continue;
    if (entry.name[prefix] != '-')
      continue;

    char path[512], status[32] = "";
    snprintf(path, sizeof(path), "/sys/class/drm/%s/status", entry.name);
    FILE *f = fopen(path, "r");
    if (!f)
      continue;
    if (fgets(status, sizeof(status), f) &&
        strncmp(status, "connected", 9) == 0)
      connected = 1;
    fclose(f);
    if (connected)
      break;
  }
  platform_dir_close(&it);
  return connected;
}

//
// Punteggio della GPU. Il tipo di dispositivo e' solo il criterio di
// spareggio: quello che decide davvero e' se la scheda possa mostrare
// l'immagine da sola (vedi gpu_has_connected_display sopra per le misure).
//
static int set_score(PHYSICAL_DEVICE device) {

  int score = 0;
  if (device.property.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
    score += 1000;
  } else if (device.property.deviceType ==
             VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU) {
    score += 100;
  } else if (device.property.deviceType ==
             VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU) {
    score += 10;
  }

  // Deve dominare la differenza fra discreta e integrata: il pedaggio di
  // presentazione misurato raddoppia il frame, mentre il vantaggio di
  // potenza della discreta valeva 2 ms su 5 di penalita'.
  if (device.ownsDisplay == 1)
    score += 10000;

  return score;
}

static PHYSICAL_DEVICE createDevice(VkPhysicalDevice device) {

  VkPhysicalDeviceProperties deviceProperties;
  vkGetPhysicalDeviceProperties(device, &deviceProperties);

  PHYSICAL_DEVICE dev = {0};
  dev.device = device;
  dev.property = deviceProperties;
  // Prima del punteggio: set_score lo legge.
  dev.ownsDisplay = gpu_has_connected_display(deviceProperties.vendorID,
                                              deviceProperties.deviceID);
  dev.deviceScore = set_score(dev);
  return dev;
}

bool find_graphics_present_queue_family(VkPhysicalDevice device,
                                        VkSurfaceKHR surface,
                                        uint32_t *out_family_index) {
  uint32_t queue_count = 0;
  vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_count, NULL);
  if (queue_count == 0)
    return false;

  VkQueueFamilyProperties queue_props[queue_count];
  vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_count, queue_props);

  for (uint32_t i = 0; i < queue_count; ++i) {
    VkBool32 present_supported = VK_FALSE;
    if (vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface,
                                             &present_supported) != VK_SUCCESS)
      continue;

    if ((queue_props[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) &&
        (queue_props[i].queueFlags & VK_QUEUE_TRANSFER_BIT) &&
        (queue_props[i].queueFlags & VK_QUEUE_COMPUTE_BIT) &&
        present_supported) {
      *out_family_index = i;
      return true;
    }
  }

  return false;
}

static bool device_supports_swapchain_extension(VkPhysicalDevice device) {
  uint32_t extension_count = 0;
  if (vkEnumerateDeviceExtensionProperties(device, NULL, &extension_count,
                                           NULL) != VK_SUCCESS)
    return false;

  if (extension_count == 0)
    return false;

  VkExtensionProperties *extensions =
      malloc(sizeof(VkExtensionProperties) * extension_count);
  if (!extensions) {
    LOG_ERROR("cannot allocate physical device extension list");
    return false;
  }
  if (vkEnumerateDeviceExtensionProperties(device, NULL, &extension_count,
                                           extensions) != VK_SUCCESS) {
    free(extensions);
    return false;
  }

  for (uint32_t i = 0; i < extension_count; ++i) {
    if (strcmp(extensions[i].extensionName, VK_KHR_SWAPCHAIN_EXTENSION_NAME) ==
        0) {
      free(extensions);
      return true;
    }
  }

  free(extensions);
  return false;
}

static bool device_has_swapchain_surface_support(VkPhysicalDevice device,
                                                 VkSurfaceKHR surface) {
  uint32_t format_count = 0;
  uint32_t present_mode_count = 0;
  VkResult formats_result = vkGetPhysicalDeviceSurfaceFormatsKHR(
      device, surface, &format_count, NULL);
  VkResult present_modes_result = vkGetPhysicalDeviceSurfacePresentModesKHR(
      device, surface, &present_mode_count, NULL);

  return formats_result == VK_SUCCESS && present_modes_result == VK_SUCCESS &&
         format_count > 0 && present_mode_count > 0;
}

static bool device_is_suitable(VkPhysicalDevice device, VkSurfaceKHR surface) {
  uint32_t queue_family_index = 0;
  return find_graphics_present_queue_family(device, surface,
                                            &queue_family_index) &&
         device_supports_swapchain_extension(device) &&
         device_has_swapchain_surface_support(device, surface);
}

/* Keep only the selected name needed by gfx_selected_gpu_name(). */
static char g_gpu_selected[256] = "";
const char *gpu_selected_name(void) { return g_gpu_selected; }

// Confronto per APP_GPU: "integrated"/"discrete" per tipo, altrimenti
// sottostringa del nome senza distinzione di maiuscole (cosi' "radeon",
// "intel" o "UHD" funzionano tutti).
static bool device_matches_request(PHYSICAL_DEVICE device, const char *want) {
  if (!want || !*want)
    return false;

  if (strcmp(want, "integrated") == 0)
    return device.property.deviceType ==
           VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU;
  if (strcmp(want, "discrete") == 0)
    return device.property.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU;

  const char *name = device.property.deviceName;
  const size_t wantLen = strlen(want);
  for (const char *start = name; *start; ++start) {
    size_t i = 0;
    while (i < wantLen && start[i]) {
      char a = start[i], b = want[i];
      if (a >= 'A' && a <= 'Z')
        a = (char)(a - 'A' + 'a');
      if (b >= 'A' && b <= 'Z')
        b = (char)(b - 'A' + 'a');
      if (a != b)
        break;
      ++i;
    }
    if (i == wantLen)
      return true;
  }
  return false;
}

// Un avviso vale piu' di un mistero: chi finisce su una GPU senza uscite
// vede un frame lento a scena vuota, un sintomo che non somiglia affatto
// alla causa. Vale anche per la scelta FORZATA — anzi soprattutto: chi
// forza a mano e' chi rischia di piu' di farlo per sbaglio.
static void warn_if_headless(PHYSICAL_DEVICE device) {
  if (device.ownsDisplay != 0)
    return;
  LOG_WARN("La GPU scelta (%s) non ha uscite video collegate: ogni frame "
           "verra' copiato su un'altra GPU per essere mostrato, con un costo "
           "fisso per frame. Per scegliere un dispositivo usare APP_GPU=<nome>.",
           device.property.deviceName);
}

VkPhysicalDevice select_device(VkInstance instance, VkSurfaceKHR surface) {

  uint32_t count = 0;
  count = count_device(instance);

  LOG_INFO("num device: %d", count);
  if (count == 0) {

    LOG_ERROR("no device suitable \n");
    return VK_NULL_HANDLE;
  }

  VkPhysicalDevice *devices = malloc((size_t)count * sizeof(*devices));
  if (!devices) return VK_NULL_HANDLE;
  if (vkEnumeratePhysicalDevices(instance, &count, devices) != VK_SUCCESS) {
    free(devices); return VK_NULL_HANDLE;
  }

  /* APP_GPU selects a suitable device by type or case-insensitive name.
   * Retain the historical environment alias for existing launch scripts. */
  const char *forced = getenv("APP_GPU");
  if (!forced || !*forced) forced = getenv("GAME_GPU");
  g_gpu_selected[0] = '\0';

  PHYSICAL_DEVICE bestdevice = {0};
  PHYSICAL_DEVICE forcedDevice = {0};
  bool found = false;
  bool forcedFound = false;
  for (uint32_t i = 0; i < count; i++) {
    PHYSICAL_DEVICE candidate = createDevice(devices[i]);
    if (!device_is_suitable(candidate.device, surface)) {
      LOG_WARN("GPU %s ignorata: manca una queue GRAPHICS+PRESENT o il "
               "supporto swapchain",
               candidate.property.deviceName);
      continue;
    }

    LOG_INFO("GPU candidata: %s | schermo collegato: %s | punteggio %d",
             candidate.property.deviceName,
             candidate.ownsDisplay == 1   ? "si"
             : candidate.ownsDisplay == 0 ? "NO (l'immagine andra' copiata "
                                            "su un'altra GPU)"
                                          : "non determinabile",
             candidate.deviceScore);

    if (forced && !forcedFound && device_matches_request(candidate, forced)) {
      forcedDevice = candidate;
      forcedFound = true;
    }
    if (!found || bestdevice.deviceScore < candidate.deviceScore) {
      bestdevice = candidate;
      found = true;
    }
  }

  free(devices);
  if (!found) {
    LOG_ERROR("no device supports graphics, present and swapchain");
    return VK_NULL_HANDLE;
  }

  if (forced) {
    if (forcedFound) {
      LOG_INFO("pick device %s (forzata da '%s')",
               forcedDevice.property.deviceName, forced);
      warn_if_headless(forcedDevice);
      snprintf(g_gpu_selected, sizeof(g_gpu_selected), "%s",
               forcedDevice.property.deviceName);
      return forcedDevice.device;
    }
    LOG_WARN("La GPU richiesta ('%s') non corrisponde a nessun dispositivo "
             "utilizzabile — uso la scelta automatica",
             forced);
  }

  warn_if_headless(bestdevice);
  LOG_INFO("pick device %s", bestdevice.property.deviceName);
  snprintf(g_gpu_selected, sizeof(g_gpu_selected), "%s",
           bestdevice.property.deviceName);
  return bestdevice.device;
}
