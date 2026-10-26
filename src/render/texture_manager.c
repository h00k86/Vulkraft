#include "render/texture_manager.h"
#include "render/gfx_vulkan.h"
#include "render/vulkan_descriptors.h"
#include "platform/platform_fs.h"
#include "log/log.h"
#include <string.h>
#include <stdio.h>
static Texture g_textures[MAX_TEXTURE_SLOTS];
static char g_paths[MAX_TEXTURE_SLOTS][256];
static bool g_slot_used[MAX_TEXTURE_SLOTS];
static long long g_lastMtime[MAX_TEXTURE_SLOTS];
static bool g_batchActive;
extern void load_texture_tiled(APP *app, const char *filename, Texture *texture);
extern void set_texture_address_mode(APP *app, Texture *texture, VkSamplerAddressMode mode);


static int texture_manager_load_internal(
    APP *app, const char *path,
    void (*loader)(APP *, const char *, Texture *)) {
  // Riusa lo slot se questo path è già caricato — esclude gli slot
  // riservati (preview transiente, offscreen dell'acqua, normal map
  // dell'acqua, i tre della barra della vita). Nota: se
  // lo stesso path è già stato caricato da una chiamata PRECEDENTE con
  // un loader diverso (tiled vs non-tiled), qui si riusa comunque quello
  // slot — il sampler resta quello del primo caricamento, questa
  // chiamata non lo "aggiorna".
  for (int i = 1; i < TEXTURE_PREVIEW_SLOT; i++) {
    if (g_slot_used[i] && strcmp(g_paths[i], path) == 0)
      return i;
  }

  // Trova il primo slot libero (0 è riservato al font; HEALTHBAR_BACK_SLOT
  // e gli slot successivi sono riservati alla barra della vita, al
  // picker e all'acqua — vedi texture_manager.h).
  int slot = -1;
  for (int i = 1; i < TEXTURE_PREVIEW_SLOT; i++) {
    if (!g_slot_used[i]) {
      slot = i;
      break;
    }
  }

  if (slot == -1) {
    /* una volta per percorso: chi la richiede a ogni frame (l'HUD) non
     * deve riempire il registro */
    static char lastFailed[8][256];
    static int nextFailed;
    bool logged = false;
    for (int k = 0; k < 8; ++k)
      if (strcmp(lastFailed[k], path) == 0)
        logged = true;
    if (!logged) {
      LOG_ERROR("Nessuno slot texture libero (max %d): %s", MAX_TEXTURE_SLOTS, path);
      snprintf(lastFailed[nextFailed++ % 8], sizeof(lastFailed[0]), "%s", path);
    }
    return -1;
  }

  // Slot pulito prima del caricamento: se il file manca il loader lascia
  // view/sampler a VK_NULL_HANDLE (vulkan_texture.c) e il controllo sotto
  // deve vederli davvero nulli, non i resti di un caricamento precedente.
  memset(&g_textures[slot], 0, sizeof(g_textures[slot]));
  loader(app, path, &g_textures[slot]);

  // Difesa: se il caricamento è fallito silenziosamente (file mancante, ecc.),
  // view/sampler restano a VK_NULL_HANDLE — non marcare lo slot come valido
  if (g_textures[slot].view == VK_NULL_HANDLE ||
      g_textures[slot].sampler == VK_NULL_HANDLE) {
    LOG_ERROR("Caricamento texture fallito: %s (slot %d scartato)", path, slot);
    return -1;
  }

  strncpy(g_paths[slot], path, sizeof(g_paths[slot]) - 1);
  g_paths[slot][sizeof(g_paths[slot]) - 1] = 0;
  g_slot_used[slot] = true;

  long long mtime = 0;
  g_lastMtime[slot] = platform_file_mtime(path, &mtime) ? mtime : 0;

  if (g_batchActive) {
    update_descriptor_texture_slot_no_wait(app, slot, &g_textures[slot]);
  } else {
    update_descriptor_texture_slot(app, slot, &g_textures[slot]);
  }

  LOG_INFO("Texture caricata: %s -> slot %d", path, slot);
  return slot;
}

int texture_manager_load(APP *app, const char *path) {
  return texture_manager_load_internal(app, path, load_texture);
}

int texture_manager_load_tiled(APP *app, const char *path) {
  int slot = texture_manager_load_internal(app, path, load_texture_tiled);
  // GARANTISCE REPEAT anche se lo slot era già stato caricato da una
  // chiamata PRECEDENTE non-tiled (texture_manager_load_internal sopra
  // riusa lo slot per path, ma non "aggiorna" il sampler di un
  // caricamento già esistente) — senza questo, un box con tiling che
  // condivide il path con un box senza tiling caricato per primo
  // restava silenziosamente con un sampler CLAMP: la UV va comunque
  // oltre [0,1], ma CLAMP blocca tutto quello che sfora sul pixel di
  // bordo invece di ripeterlo.
  if (slot != -1)
    texture_manager_ensure_repeat(app, slot);
  return slot;
}

void texture_manager_ensure_repeat(APP *app, int slot) {
  if (slot <= 0 || slot >= MAX_TEXTURE_SLOTS || !g_slot_used[slot])
    return;

  // Il sampler corrente puo' essere ancora referenziato dai descriptor set
  // dei frame in volo. Attendere PRIMA di distruggerlo, non dopo
  // l'aggiornamento del descriptor: vkDestroySampler non e' sicuro finche'
  // la GPU puo' ancora usarlo.
  vkDeviceWaitIdle(VK_CTX(app)->logical_device);
  set_texture_address_mode(app, &g_textures[slot],
                           VK_SAMPLER_ADDRESS_MODE_REPEAT);
  update_descriptor_texture_slot_no_wait(app, slot, &g_textures[slot]);
  // 2026-09-18: anche il set delle particelle referenzia il sampler dello
  // slot (particles_update_texture_slot al caricamento): senza questo
  // aggiornamento restava un sampler distrutto (VUID-vkCmdDraw-None-08114
  // appena un kit con texture tiled si caricava dopo i VFX).
}

bool texture_manager_is_valid(int slot) {
  return slot > 0 && slot < MAX_TEXTURE_SLOTS && g_slot_used[slot];
}

int texture_manager_free_slots(void) {
  int free = 0;
  for (int i = 1; i < TEXTURE_PREVIEW_SLOT; i++)
    if (!g_slot_used[i])
      free++;
  return free;
}

int texture_manager_loaded_count(void) {
  int count = 0;
  for (int i = 1; i < MAX_TEXTURE_SLOTS; i++) {
    if (g_slot_used[i])
      count++;
  }
  return count;
}

bool texture_manager_check_hot_reload(APP *app, int slot) {
  if (slot <= 0 || slot >= MAX_TEXTURE_SLOTS || !g_slot_used[slot])
    return false;

  long long mtime = 0;
  if (!platform_file_mtime(g_paths[slot], &mtime))
    return false; // file momentaneamente irraggiungibile (es. Krita lo sta
                  // riscrivendo proprio ora) — ignora, ci riprova il
                  // prossimo frame

  if (mtime == g_lastMtime[slot])
    return false; // nessuna modifica da quando l'abbiamo letta l'ultima volta

  g_lastMtime[slot] = mtime;

  // Ricarica sul posto: stesso slot, path invariato — solo i pixel
  // cambiano. destroy_texture prima di load_texture, altrimenti la
  // vecchia VkImage/VkImageView/VkSampler perderebbero il riferimento
  // senza mai essere liberate.
  vkDeviceWaitIdle(VK_CTX(app)->logical_device);
  destroy_texture(app, &g_textures[slot]);
  load_texture(app, g_paths[slot], &g_textures[slot]);

  if (g_textures[slot].view == VK_NULL_HANDLE ||
      g_textures[slot].sampler == VK_NULL_HANDLE) {
    LOG_ERROR("Hot-reload fallito per %s (slot %d)", g_paths[slot], slot);
    g_slot_used[slot] = false;
    return false;
  }

  update_descriptor_texture_slot(app, slot, &g_textures[slot]);

  LOG_INFO("Texture ricaricata a caldo: %s (slot %d)", g_paths[slot], slot);
  return true;
}

void texture_manager_begin_batch(APP *app) {
  vkDeviceWaitIdle(VK_CTX(app)->logical_device);
  g_batchActive = true;
}

void texture_manager_end_batch(void) { g_batchActive = false; }

void texture_manager_unload_all(APP *app) {
  vkDeviceWaitIdle(
      VK_CTX(app)->logical_device); // nessun frame in volo la usa più —
                                     // un solo wait copre TUTTO il ciclo
                                     // sotto, nessuna nuova sottomissione
                                     // alla GPU avviene nel frattempo

  for (int i = 1; i < MAX_TEXTURE_SLOTS; i++) {
    if (g_slot_used[i]) {
      destroy_texture(app, &g_textures[i]);
      g_slot_used[i] = false;
      g_paths[i][0] = 0;

      // Senza questo, il descriptor set continuava a puntare alle
      // risorse (imageView/sampler) appena distrutte da destroy_texture
      // qui sopra — "invalid or has been destroyed" alla prima draw
      // call che tocca questo slot, finché qualcosa non ricaricava per
      // puro caso una texture nuova proprio in questo stesso slot.
      // Stesso placeholder sicuro usato da create_descriptor_sets per
      // inizializzare tutti gli slot all'avvio. _no_wait: il wait
      // all'inizio di questa funzione copre già l'intero ciclo, un
      // altro per ogni slot singolo sarebbe stato ridondante.
      update_descriptor_texture_slot_no_wait(app, i, &app->renderer.gpu->texture);
    }
  }
}

void texture_manager_shutdown(APP *app) {
  texture_manager_unload_all(app);
  g_batchActive = false;
}

const char *texture_manager_get_path(int slot) {
  if (slot <= 0 || slot >= MAX_TEXTURE_SLOTS || !g_slot_used[slot])
    return NULL;
  return g_paths[slot];
}

bool texture_manager_get_size(int slot, int *outW, int *outH) {
  if (slot <= 0 || slot >= MAX_TEXTURE_SLOTS || !g_slot_used[slot])
    return false;
  *outW = (int)g_textures[slot].width;
  *outH = (int)g_textures[slot].height;
  return true;
}

int texture_manager_preview_load(APP *app, const char *path) {
  if (!app || !path || !path[0])
    return -1;

  for (int i = 1; i < TEXTURE_PREVIEW_SLOT; ++i) {
    if (g_slot_used[i] && strcmp(g_paths[i], path) == 0)
      return i;
  }
  if (g_slot_used[TEXTURE_PREVIEW_SLOT] &&
      strcmp(g_paths[TEXTURE_PREVIEW_SLOT], path) == 0)
    return TEXTURE_PREVIEW_SLOT;

  if (g_slot_used[TEXTURE_PREVIEW_SLOT])
    texture_manager_preview_clear(app);

  memset(&g_textures[TEXTURE_PREVIEW_SLOT], 0,
         sizeof(g_textures[TEXTURE_PREVIEW_SLOT]));
  load_texture(app, path, &g_textures[TEXTURE_PREVIEW_SLOT]);
  if (g_textures[TEXTURE_PREVIEW_SLOT].view == VK_NULL_HANDLE ||
      g_textures[TEXTURE_PREVIEW_SLOT].sampler == VK_NULL_HANDLE) {
    memset(&g_textures[TEXTURE_PREVIEW_SLOT], 0,
           sizeof(g_textures[TEXTURE_PREVIEW_SLOT]));
    return -1;
  }
  snprintf(g_paths[TEXTURE_PREVIEW_SLOT],
           sizeof(g_paths[TEXTURE_PREVIEW_SLOT]), "%s", path);
  g_slot_used[TEXTURE_PREVIEW_SLOT] = true;
  update_descriptor_texture_slot(app, TEXTURE_PREVIEW_SLOT,
                                 &g_textures[TEXTURE_PREVIEW_SLOT]);
  return TEXTURE_PREVIEW_SLOT;
}

void texture_manager_preview_clear(APP *app) {
  if (!app || !g_slot_used[TEXTURE_PREVIEW_SLOT])
    return;
  vkDeviceWaitIdle(VK_CTX(app)->logical_device);
  destroy_texture(app, &g_textures[TEXTURE_PREVIEW_SLOT]);
  memset(&g_textures[TEXTURE_PREVIEW_SLOT], 0,
         sizeof(g_textures[TEXTURE_PREVIEW_SLOT]));
  g_paths[TEXTURE_PREVIEW_SLOT][0] = '\0';
  g_slot_used[TEXTURE_PREVIEW_SLOT] = false;
  update_descriptor_texture_slot_no_wait(app, TEXTURE_PREVIEW_SLOT,
                                         &app->renderer.gpu->texture);
}
