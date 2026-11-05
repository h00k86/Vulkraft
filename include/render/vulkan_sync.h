#pragma once
#include <stdbool.h>

#include <stdlib.h>

#include <vulkan/vulkan.h>

typedef struct APP APP;
#include "log/log.h"



// Dichiarazioni delle funzioni per la gestione degli oggetti di sincronizzazione
bool create_sync_objects(APP *app);
void destroy_sync_objects(APP *app);

// Rialloca gli oggetti dimensionati sulla swapchain (fence per immagine e
// semafori per immagine). Da chiamare dopo ogni ricreazione della
// swapchain: il numero di immagini puo' cambiare. Vive qui e non in
// vulkan_swapchain.c perche' un tempo la stessa logica era duplicata nei
// due file, e le due copie sono andate fuori sincrono.
bool sync_resize_swapchain_objects(APP *app);
