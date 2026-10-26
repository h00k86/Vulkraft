#pragma once
#include "render/gfx.h" /* PORTING E14: tipi opachi, niente Vulkan */

/* MACROTASK MULTI-GIOCO (2026-09-17): dump diagnostico dei frame presentati.
 * APP_FRAME_DUMP_DIR=<cartella> [APP_FRAME_DUMP_EVERY=<n frame>, default 30]
 * [APP_FRAME_DUMP_FPS=<n>: cadenza sul tempo simulato invece che sui frame]
 * [APP_FRAME_DUMP_MAX=<n file>]: ogni n frame il command buffer del frame
 * copia l'immagine della swapchain (dopo l'ultimo render pass, HUD
 * compreso) in un buffer host-visible dello stesso slot in volo; quando la
 * fence di quello slot e' passata, il buffer viene scritto come PPM
 * (frame_%05d.ppm). Nessun costo senza la variabile. Serve alla verifica
 * visiva senza cattura dello schermo (display bloccato, sessione remota). */

typedef struct APP APP;

void frame_dump_record(APP *app, GfxCommandList cmd, uint32_t frame_index,
                       uint32_t image_index);
void frame_dump_collect(APP *app, uint32_t frame_index);
void frame_dump_shutdown(APP *app);
