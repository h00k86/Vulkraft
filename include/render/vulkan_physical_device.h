


#pragma once
#include <stdbool.h>
#include <vulkan/vulkan.h>
#include <stdlib.h>

#include "log/log.h"



typedef struct {

    VkPhysicalDevice device ;
    VkPhysicalDeviceProperties property;
    VkPhysicalDeviceFeatures feature;
    int deviceScore;
    // 1 = questa GPU ha almeno un'uscita video COLLEGATA, 0 = nessuna,
    // -1 = non determinabile. Vedi gpu_has_connected_display().
    int ownsDisplay;
}PHYSICAL_DEVICE;


// Trova una queue family singola utilizzabile sia dal rendering sia dalla
// presentazione. Il renderer usa intenzionalmente una sola queue.
bool find_graphics_present_queue_family(VkPhysicalDevice device,
                                        VkSurfaceKHR surface,
                                        uint32_t *out_family_index);

VkPhysicalDevice select_device(VkInstance instance, VkSurfaceKHR surface);

// --- Elenco delle GPU utilizzabili, per la schermata OPZIONI ---
//
// Riempito da select_device durante l'inizializzazione: contiene solo i
// dispositivi che hanno superato i controlli (queue GRAPHICS+PRESENT e
// swapchain), cioe' quelli davvero selezionabili. Prima di init_vulkan
// l'elenco e' vuoto.
int gpu_available_count(void);
const char *gpu_available_name(int index);
// Nome del dispositivo effettivamente in uso, "" se non ancora scelto.
const char *gpu_selected_name(void);

// Preferenza dell'utente: nome (anche parziale, senza distinzione di
// maiuscole) oppure "integrated"/"discrete"; NULL o stringa vuota =
// scelta automatica. Va impostata PRIMA di init_vulkan — il dispositivo si
// sceglie una volta sola. La variabile d'ambiente GAME_GPU ha comunque la
// precedenza, cosi' una prova al volo non richiede di toccare le opzioni.
void gpu_set_preference(const char *name);
