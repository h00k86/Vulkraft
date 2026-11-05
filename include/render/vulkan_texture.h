#pragma once

#include <vulkan/vulkan.h>
#include <stdint.h>


typedef struct APP APP;


typedef struct Texture {

    VkImage image;
    VkDeviceMemory memory;

    VkImageView view;
    VkSampler sampler;

    uint32_t width;
    uint32_t height;

    /* Livelli di mipmap generati (1 = nessuna). Vedi vulkan_texture.c:
       le hanno solo le texture RIPETUTE, cioe' le superfici del mondo;
       gli atlas restano a un livello. */
    uint32_t mipLevels;

    /* Preserve sampling policy when replacing an image during hot reload. */
    VkSamplerAddressMode address_mode;

} Texture;



void load_texture(
        APP *app,
        const char *filename,
        Texture *texture
);



void destroy_texture(
        APP *app,
        Texture *texture
);
