# Widget indipendenti da APP

Le API dei widget usano il `UIContext` corrente e il renderer associato.
`ui/ui.h`, `ui/file_picker.h` e il core non dichiarano né utilizzano il tipo `APP`.
Il controllo architetturale impedisce di reintrodurlo fuori dall'adattatore.

## Migrazione

Questo aggiornamento cambia le firme C e richiede di ricompilare librerie e
consumatori. Rimuovere il primo argomento APP (oppure NULL) dalle chiamate ai
widget, compresi property/layout, menu, notifiche, popup e file picker:

```c
/* Prima */
ui_property_text(app, &layout, "name", font, "Nome", buffer, sizeof buffer);
ui_draw_dropdown_popups(app);
ui_file_picker_draw(app, font, path, sizeof path);

/* Ora */
ui_property_text(&layout, "name", font, "Nome", buffer, sizeof buffer);
ui_draw_dropdown_popups();
ui_file_picker_draw(font, path, sizeof path);
```

Anche `ui_debug_log_input_events()` ora non riceve argomenti; usa esclusivamente
lo stato del frame corrente. La configurazione UI_INPUT_DEBUG resta invariata.

`ui_begin_frame(APP*, ...)`, `ui_bind_app_renderer(APP*)` e `ui_app_renderer()`
appartengono a `ui/app_adapter.h`: includerlo quando si usa l'adattatore
applicativo, per esempio in un ciclo personalizzato. Le API del renderer Vulkan
mantengono i propri parametri APP. Per un backend indipendente usare
`ui_begin_frame_input()` e `ui_context_set_renderer()`.

Esempi, picker e test del repository sono migrati. Restano invariati il modello
a contesto corrente e i vincoli di durata delle risorse del backend.

Le verifiche di questo aggiornamento sono in
[evidence/ui-widget-api](../evidence/ui-widget-api/). I test del picker usano
input e disegno simulati; non costituiscono una prova visiva del dialogo Vulkan.

Verifiche completate: 24 eseguibili di test, 21 con ASan/UBSan, build debug,
release e headless senza warning. Runtime Vulkan con draw list: 30 frame,
tre ridimensionamenti, VSync e starter, senza errori di validazione.
LeakSanitizer non eseguito.
