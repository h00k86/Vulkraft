#include "render/vulkan_memory_tracker.h"
#include "render/gfx_vulkan.h"

#include <inttypes.h>
#include <stdlib.h>
#include <string.h>

#ifndef DEBUG

void vulkan_memory_track_allocation(
    VkPhysicalDevice physical_device, VkDeviceMemory memory,
    VulkanMemoryResourceKind kind, const char *name,
    VkDeviceSize requested_bytes, const VkMemoryRequirements *requirements,
    uint32_t memory_type_index, int frame_index, bool persistent) {
  (void)physical_device;
  (void)memory;
  (void)kind;
  (void)name;
  (void)requested_bytes;
  (void)requirements;
  (void)memory_type_index;
  (void)frame_index;
  (void)persistent;
}

void vulkan_memory_track_image_info(VkDeviceMemory memory, uint32_t width,
                                    uint32_t height, VkFormat format,
                                    uint32_t mip_levels,
                                    uint32_t array_layers) {
  (void)memory;
  (void)width;
  (void)height;
  (void)format;
  (void)mip_levels;
  (void)array_layers;
}

void vulkan_memory_track_label(VkDeviceMemory memory, const char *name,
                               int frame_index, bool persistent) {
  (void)memory;
  (void)name;
  (void)frame_index;
  (void)persistent;
}

void vulkan_memory_track_free(VkDeviceMemory memory) { (void)memory; }

VulkanMemorySnapshot vulkan_memory_snapshot(void) {
  VulkanMemorySnapshot snapshot = {0};
  return snapshot;
}

void vulkan_memory_write_inventory(FILE *stream,
                                   VkPhysicalDevice physical_device,
                                   const char *label) {
  (void)stream;
  (void)physical_device;
  (void)label;
}

#else

#define MAX_TRACKED_VULKAN_ALLOCATIONS 512u
#define TRACKED_NAME_CAPACITY 96u

typedef struct {
  bool occupied;
  bool active;
  VkDeviceMemory memory;
  VulkanMemoryResourceKind kind;
  char name[TRACKED_NAME_CAPACITY];
  VkDeviceSize requested_bytes;
  VkDeviceSize allocation_bytes;
  VkDeviceSize alignment;
  VkMemoryPropertyFlags properties;
  uint32_t memory_type_index;
  uint32_t heap_index;
  int frame_index;
  bool persistent;
  uint32_t width;
  uint32_t height;
  VkFormat format;
  uint32_t mip_levels;
  uint32_t array_layers;
} TrackedAllocation;

static TrackedAllocation g_allocations[MAX_TRACKED_VULKAN_ALLOCATIONS];
static uint64_t g_allocation_events;
static uint64_t g_free_events;
static uint64_t g_history_reuse_events;
static uint32_t g_peak_active_allocations;
static VkDeviceSize g_peak_tracked_bytes;
static bool g_capacity_warning_emitted;

static TrackedAllocation *find_allocation(VkDeviceMemory memory) {
  if (memory == VK_NULL_HANDLE)
    return NULL;
  for (uint32_t i = 0; i < MAX_TRACKED_VULKAN_ALLOCATIONS; ++i) {
    if (g_allocations[i].occupied && g_allocations[i].active &&
        g_allocations[i].memory == memory)
      return &g_allocations[i];
  }
  return NULL;
}

VulkanMemorySnapshot vulkan_memory_snapshot(void) {
  VulkanMemorySnapshot snapshot = {0};
  for (uint32_t i = 0; i < MAX_TRACKED_VULKAN_ALLOCATIONS; ++i) {
    const TrackedAllocation *allocation = &g_allocations[i];
    if (!allocation->occupied || !allocation->active)
      continue;
    snapshot.tracked_bytes += allocation->allocation_bytes;
    if (allocation->properties & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
      snapshot.device_local_bytes += allocation->allocation_bytes;
    if (allocation->properties & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)
      snapshot.host_visible_bytes += allocation->allocation_bytes;
    if (allocation->heap_index < VK_MAX_MEMORY_HEAPS)
      snapshot.heap_bytes[allocation->heap_index] +=
          allocation->allocation_bytes;
    ++snapshot.active_allocations;
  }
  return snapshot;
}

static void update_peaks(void) {
  VulkanMemorySnapshot snapshot = vulkan_memory_snapshot();
  if (snapshot.active_allocations > g_peak_active_allocations)
    g_peak_active_allocations = snapshot.active_allocations;
  if (snapshot.tracked_bytes > g_peak_tracked_bytes)
    g_peak_tracked_bytes = snapshot.tracked_bytes;
}

void vulkan_memory_track_allocation(
    VkPhysicalDevice physical_device, VkDeviceMemory memory,
    VulkanMemoryResourceKind kind, const char *name,
    VkDeviceSize requested_bytes, const VkMemoryRequirements *requirements,
    uint32_t memory_type_index, int frame_index, bool persistent) {
  if (memory == VK_NULL_HANDLE || !requirements)
    return;

  TrackedAllocation *allocation = NULL;
  for (uint32_t i = 0; i < MAX_TRACKED_VULKAN_ALLOCATIONS; ++i) {
    if (!g_allocations[i].occupied) {
      allocation = &g_allocations[i];
      break;
    }
  }
  if (!allocation) {
    for (uint32_t i = 0; i < MAX_TRACKED_VULKAN_ALLOCATIONS; ++i) {
      if (!g_allocations[i].active) {
        allocation = &g_allocations[i];
        ++g_history_reuse_events;
        break;
      }
    }
  }
  if (!allocation) {
    if (!g_capacity_warning_emitted) {
      fprintf(stderr,
              "[VRAM] allocation tracker has %u simultaneous active "
              "allocations; additional records unavailable\n",
              MAX_TRACKED_VULKAN_ALLOCATIONS);
      g_capacity_warning_emitted = true;
    }
    return;
  }

  VkPhysicalDeviceMemoryProperties memory_properties;
  vkGetPhysicalDeviceMemoryProperties(physical_device, &memory_properties);
  if (memory_type_index >= memory_properties.memoryTypeCount) {
    fprintf(stderr, "[VRAM] invalid memory type index %u\n",
            memory_type_index);
    return;
  }

  memset(allocation, 0, sizeof(*allocation));
  allocation->occupied = true;
  allocation->active = true;
  allocation->memory = memory;
  allocation->kind = kind;
  snprintf(allocation->name, sizeof(allocation->name), "%s",
           name ? name : "unnamed");
  allocation->requested_bytes = requested_bytes;
  allocation->allocation_bytes = requirements->size;
  allocation->alignment = requirements->alignment;
  allocation->properties =
      memory_properties.memoryTypes[memory_type_index].propertyFlags;
  allocation->memory_type_index = memory_type_index;
  allocation->heap_index =
      memory_properties.memoryTypes[memory_type_index].heapIndex;
  allocation->frame_index = frame_index;
  allocation->persistent = persistent;
  ++g_allocation_events;
  update_peaks();
}

void vulkan_memory_track_image_info(VkDeviceMemory memory, uint32_t width,
                                    uint32_t height, VkFormat format,
                                    uint32_t mip_levels,
                                    uint32_t array_layers) {
  TrackedAllocation *allocation = find_allocation(memory);
  if (!allocation)
    return;
  allocation->width = width;
  allocation->height = height;
  allocation->format = format;
  allocation->mip_levels = mip_levels;
  allocation->array_layers = array_layers;
}

void vulkan_memory_track_label(VkDeviceMemory memory, const char *name,
                               int frame_index, bool persistent) {
  TrackedAllocation *allocation = find_allocation(memory);
  if (!allocation)
    return;
  snprintf(allocation->name, sizeof(allocation->name), "%s",
           name ? name : "unnamed");
  allocation->frame_index = frame_index;
  allocation->persistent = persistent;
}

void vulkan_memory_track_free(VkDeviceMemory memory) {
  TrackedAllocation *allocation = find_allocation(memory);
  if (!allocation || !allocation->active)
    return;
  allocation->active = false;
  ++g_free_events;
}

static const char *kind_name(VulkanMemoryResourceKind kind) {
  return kind == VULKAN_MEMORY_RESOURCE_IMAGE ? "image" : "buffer";
}

static void properties_string(VkMemoryPropertyFlags properties, char *out,
                              size_t out_size) {
  out[0] = '\0';
  if (properties & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
    strncat(out, "DEVICE_LOCAL|", out_size - strlen(out) - 1u);
  if (properties & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)
    strncat(out, "HOST_VISIBLE|", out_size - strlen(out) - 1u);
  if (properties & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)
    strncat(out, "HOST_COHERENT|", out_size - strlen(out) - 1u);
  if (properties & VK_MEMORY_PROPERTY_HOST_CACHED_BIT)
    strncat(out, "HOST_CACHED|", out_size - strlen(out) - 1u);
  size_t length = strlen(out);
  if (length > 0 && out[length - 1u] == '|')
    out[length - 1u] = '\0';
  if (out[0] == '\0')
    snprintf(out, out_size, "0");
}

void vulkan_memory_write_inventory(FILE *stream,
                                   VkPhysicalDevice physical_device,
                                   const char *label) {
  if (!stream)
    return;

  bool has_memory_budget = false;
  uint32_t extension_count = 0;
  if (vkEnumerateDeviceExtensionProperties(physical_device, NULL,
                                           &extension_count, NULL) ==
          VK_SUCCESS &&
      extension_count > 0) {
    VkExtensionProperties *extensions =
        calloc(extension_count, sizeof(*extensions));
    if (extensions &&
        vkEnumerateDeviceExtensionProperties(physical_device, NULL,
                                             &extension_count, extensions) ==
            VK_SUCCESS) {
      for (uint32_t i = 0; i < extension_count; ++i) {
        if (strcmp(extensions[i].extensionName,
                   VK_EXT_MEMORY_BUDGET_EXTENSION_NAME) == 0) {
          has_memory_budget = true;
          break;
        }
      }
    }
    free(extensions);
  }
  VkPhysicalDeviceMemoryProperties2 properties2 = {
      .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_PROPERTIES_2};
  VkPhysicalDeviceMemoryBudgetPropertiesEXT budget = {
      .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_BUDGET_PROPERTIES_EXT};
  properties2.pNext = has_memory_budget ? &budget : NULL;
  vkGetPhysicalDeviceMemoryProperties2(physical_device, &properties2);

  VulkanMemorySnapshot snapshot = vulkan_memory_snapshot();
  fprintf(stream, "\nVULKAN MEMORY INVENTORY — %s\n",
          label ? label : "snapshot");
  fprintf(stream,
          "active_allocations=%u allocation_events=%" PRIu64
          " free_events=%" PRIu64 " history_reuses=%" PRIu64
          " peak_active=%u\n",
          snapshot.active_allocations, g_allocation_events, g_free_events,
          g_history_reuse_events, g_peak_active_allocations);
  fprintf(stream,
          "name,type,state,requested_bytes,allocation_bytes,alignment,"
          "memory_type,heap,properties,frame,lifetime,image\n");
  for (uint32_t i = 0; i < MAX_TRACKED_VULKAN_ALLOCATIONS; ++i) {
    const TrackedAllocation *allocation = &g_allocations[i];
    if (!allocation->occupied)
      continue;
    char property_text[96];
    properties_string(allocation->properties, property_text,
                      sizeof(property_text));
    char image_text[96] = "NA";
    if (allocation->kind == VULKAN_MEMORY_RESOURCE_IMAGE) {
      snprintf(image_text, sizeof(image_text), "%ux%u format=%d mips=%u layers=%u",
               allocation->width, allocation->height, allocation->format,
               allocation->mip_levels, allocation->array_layers);
    }
    fprintf(stream,
            "%s,%s,%s,%" PRIu64 ",%" PRIu64 ",%" PRIu64
            ",%u,%u,%s,%d,%s,%s\n",
            allocation->name, kind_name(allocation->kind),
            allocation->active ? "active" : "freed",
            (uint64_t)allocation->requested_bytes,
            (uint64_t)allocation->allocation_bytes,
            (uint64_t)allocation->alignment, allocation->memory_type_index,
            allocation->heap_index, property_text, allocation->frame_index,
            allocation->persistent ? "persistent" : "temporary", image_text);
  }

  fprintf(stream,
          "\nACTIVE RESOURCE SUMMARY\n"
          "name,type,count,size_each_bytes,total_bytes,memory_type,heap,"
          "properties,duplication,lifetime\n");
  for (uint32_t i = 0; i < MAX_TRACKED_VULKAN_ALLOCATIONS; ++i) {
    const TrackedAllocation *first = &g_allocations[i];
    if (!first->occupied || !first->active)
      continue;
    bool already_written = false;
    for (uint32_t previous = 0; previous < i; ++previous) {
      const TrackedAllocation *candidate = &g_allocations[previous];
      if (candidate->occupied && candidate->active &&
          candidate->kind == first->kind &&
          candidate->memory_type_index == first->memory_type_index &&
          strcmp(candidate->name, first->name) == 0) {
        already_written = true;
        break;
      }
    }
    if (already_written)
      continue;

    uint32_t count = 0;
    VkDeviceSize total = 0;
    bool size_varies = false;
    bool has_frame_copy = false;
    for (uint32_t j = i; j < MAX_TRACKED_VULKAN_ALLOCATIONS; ++j) {
      const TrackedAllocation *candidate = &g_allocations[j];
      if (!candidate->occupied || !candidate->active ||
          candidate->kind != first->kind ||
          candidate->memory_type_index != first->memory_type_index ||
          strcmp(candidate->name, first->name) != 0)
        continue;
      ++count;
      total += candidate->allocation_bytes;
      size_varies |= candidate->allocation_bytes != first->allocation_bytes;
      has_frame_copy |= candidate->frame_index >= 0;
    }
    char property_text[96];
    properties_string(first->properties, property_text,
                      sizeof(property_text));
    fprintf(stream, "%s,%s,%u,", first->name, kind_name(first->kind), count);
    if (size_varies)
      fputs("varies,", stream);
    else
      fprintf(stream, "%" PRIu64 ",", (uint64_t)first->allocation_bytes);
    fprintf(stream, "%" PRIu64 ",%u,%u,%s,%s,%s\n", (uint64_t)total,
            first->memory_type_index, first->heap_index, property_text,
            has_frame_copy ? "per-frame/image" : "shared",
            first->persistent ? "persistent" : "temporary");
  }

  fprintf(stream, "\nHEAP SUMMARY\n");
  for (uint32_t heap = 0;
       heap < properties2.memoryProperties.memoryHeapCount; ++heap) {
    VkDeviceSize tracked = snapshot.heap_bytes[heap];
    if (!has_memory_budget) {
      fprintf(stream,
              "heap=%u flags=0x%x tracked_bytes=%" PRIu64
              " usage_bytes=NA budget_bytes=NA unaccounted_bytes=NA\n",
              heap, properties2.memoryProperties.memoryHeaps[heap].flags,
              (uint64_t)tracked);
      continue;
    }
    VkDeviceSize usage = budget.heapUsage[heap];
    VkDeviceSize unaccounted = usage > tracked ? usage - tracked : 0;
    fprintf(stream,
            "heap=%u flags=0x%x tracked_bytes=%" PRIu64
            " usage_bytes=%" PRIu64 " budget_bytes=%" PRIu64
            " unaccounted_bytes=%" PRIu64 "\n",
            heap, properties2.memoryProperties.memoryHeaps[heap].flags,
            (uint64_t)tracked, (uint64_t)usage,
            (uint64_t)budget.heapBudget[heap], (uint64_t)unaccounted);
  }
  fprintf(stream,
          "ENGINE_TRACKED_TOTAL_BYTES=%" PRIu64
          "\nENGINE_TRACKED_DEVICE_LOCAL_BYTES=%" PRIu64
          "\nENGINE_TRACKED_HOST_VISIBLE_BYTES=%" PRIu64
          "\nPEAK_TRACKED_TOTAL_BYTES=%" PRIu64 "\n",
          (uint64_t)snapshot.tracked_bytes,
          (uint64_t)snapshot.device_local_bytes,
          (uint64_t)snapshot.host_visible_bytes,
          (uint64_t)g_peak_tracked_bytes);
  fflush(stream);
}

#endif
