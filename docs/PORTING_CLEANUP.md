# Pulizia dei residui del porting

## Ambito e criterio

Controllati sorgenti/header C, backend, shader, esempi, test, regole Make/CMake,
strumenti Python e asset font. La ricerca dei riferimenti include chiamate e
registrazioni di callback. Il conteggio testuale segnala candidati, ma non prova
che un simbolo dell'SDK sia inutile per applicazioni esterne.

## Rimosso

| Residuo | Motivo |
| --- | --- |
| `draw_textured_quad_tinted` | Nessuna dichiarazione pubblica e nessun riferimento. La tinta rimane disponibile tramite `draw_textured_quad_uv4`. |
| `update_ubo_camera` | Helper privato mai chiamato: il frame aggiorna già l'UBO direttamente. |
| `RenderCommandStats` e relativo getter | Contatori privati scritti a ogni frame, mai letti da alcun consumatore. |
| Inventario della memoria Vulkan | Modulo privato con tabella, metadati e statistiche; nessuna chiamata al report. Rimossi il modulo e gli hook di sola contabilità. Restano allocazioni/liberazioni Vulkan e test del loro bilanciamento. |
| Cache GPU per la schermata opzioni | Getter/setter privati senza utilizzatori, array di nomi e preferenza mai impostata. Rimangono negoziazione Vulkan, nome GPU selezionata e override d'ambiente. |
| Campi GPU scritti ma mai letti | Cache delle feature nel selettore GPU, dimensione duplicata nell’UBO e tre contatori privati del vertex buffer. Restano la query effettiva delle feature nel device logico e la diagnostica pubblica della geometria CPU. |
| Variante additive della pipeline | Unico chiamante con valore sempre false. Rimane lo stesso alpha blending. |
| Ciclo su un array di un solo buffer di upload | Rimane solo il buffer UI: registrazione e barriera ora lo trattano direttamente. |
| `assets/fonts/font2.json` e `font2.png` | L'app usa `font_utf8.*`; il vecchio PNG non era referenziato e il JSON serviva solo al test del parser, ora migrato al font distribuito. |
| Commenti e opzioni di compilazione storici | Eliminati riferimenti a moduli assenti, vecchi task, schermo diviso, minimappa, acqua e Performance Lab. |

La build Make ricrea l'archivio statico dai membri attuali: `ar rcs` da solo
conservava gli oggetti di sorgenti eliminati. La modifica evita che il vecchio
modulo di inventario rimanga nell'archivio dopo la rimozione.

## Mantenuto perché necessario o parte del contratto

- `src/math/matrix.c`: il frame usa `mat4_identity` e `mat4_ortho_world_z`.
  Le altre dieci funzioni sono API installate. Non vengono cancellate in base
  alla sola assenza di chiamanti nel checkout; i commenti sono stati aggiornati.
- `Vec2` è usato da UI e primitive; `Vec3` è un tipo pubblico attualmente non
  usato internamente, senza costo di esecuzione.
- Primitive linee/cerchi/quad, texture ripetute, trasformazioni e API filesystem
  restano funzionalità generiche, anche quando gli esempi non le invocano.
- Lo stile paper è opt-in, collegato ai widget e coperto dalle prove specifiche.
  Non è codice morto. I nomi `draw_hud_*` e i campi dei renderer restano compatibili.
- Nomi pubblici `paths_engine_*`, `WORLD_Z_*` e `mat4_ortho_world_z`: compatibilità
  del codice applicativo. La root interna ora si chiama `g_resourceRoot`.
- `APP_GPU` è il nome corrente dell'override GPU. `GAME_GPU` resta come alias
  precedente, usato soltanto se APP_GPU è assente o vuoto. Gli altri messaggi e
  suggerimenti non rimandano più a schermate assenti.
- `app_loop`/`app_draw` restano l'adattatore di compatibilità documentato.
- Cattura dei frame, backend headless, test e strumenti di validazione sono
  utilizzati per verificare il framework; non appartengono al codice di gioco.

## Simboli senza riferimenti interni oltre a definizione/dichiarazione

Inventario indicativo delle API non statiche (esclusi i backend piattaforma).
I seguenti simboli sono **conservati**; questa tabella non è una lista di bug.
Le callback registrate nei backend non vengono classificate come codice morto.

| File | Simboli |
| --- | --- |
| `src/math/matrix.c` | `mat4_multiply`, `mat4_ortho`, `mat4_rotation_x`, `mat4_rotation_y`, `mat4_rotation_z`, `mat4_scale_around`, `mat4_scaling`, `mat4_transform_point`, `mat4_translate`, `mat4_translation` |
| `src/platform/paths.c` | `paths_engine_dir`, `paths_settings_file`, `paths_user_data_dir` |
| `src/render/gfx.c` | `gfx_current_frame`, `gfx_selected_gpu_name`, `gfx_surface_size`, `gfx_wait_idle` |
| `src/render/text_renderer.c` | `draw_hud_circle`, `draw_hud_star_badge`, `draw_text_anchored`, `draw_text_tilted`, `draw_textured_disc`, `draw_textured_quad_points`, `draw_textured_quad_points_alpha`, `text_clip_stack_balanced`, `text_hud_get_geometry_diagnostics`, `text_hud_transform` |
| `src/render/texture_manager.c` | `texture_manager_begin_batch`, `texture_manager_end_batch`, `texture_manager_free_slots`, `texture_manager_get_path`, `texture_manager_load`, `texture_manager_load_tiled`, `texture_manager_loaded_count` |
| `src/ui/ui.c` | `ui_any_hover`, `ui_draw_notifications`, `ui_input_region_at`, `ui_input_region_contains`, `ui_input_region_count`, `ui_mouse_down`, `ui_set_feedback`, `ui_set_notification_top`, `ui_version_string` |
| `src/ui/ui_buttons.c` | `ui_button_styled` |
| `src/ui/ui_capture.c` | `ui_has_blocking_overlay` |
| `src/ui/ui_layout.c` | `ui_layout_end_nested`, `ui_layout_indent` |
| `src/ui/ui_panels.c` | `ui_collapsible_header`, `ui_section_begin_collapsed` |
| `src/ui/ui_theme.c` | `ui_theme_warm` |

## Verifiche

Esiti automatici e limiti della sessione sono raccolti in
`evidence/porting-cleanup/README.md` (output locale, non incluso nel repository).
La verifica degli SDK riguarda sorgenti ricostruiti fuori dal checkout.
Non si afferma che tutte le API esportate abbiano copertura funzionale completa,
né che l'assenza di chiamate testuali escluda consumatori esterni.
