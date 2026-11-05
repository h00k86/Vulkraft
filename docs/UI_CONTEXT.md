# Contesti UI

Ogni `UIContext` possiede lo stato persistente dei widget: focus, campi testuali,
snapshot UTF-8, popup, menu, modali, scroll, drag, ID, tema, notifiche e callback
feedback. Gli stessi ID possono essere usati in contesti diversi senza condividere
questi stati. Anche i flag di consumo degli eventi appartengono al contesto.

## Uso con APP

`app_init()` crea `app->ui`; se l'allocazione fallisce, libera le risorse grafiche
e restituisce `APP_ERROR_MEMORY`. `app_loop()` seleziona questo contesto e
ripristina il precedente all'uscita, anche in caso di errore. Dentro `app_draw()`
le chiamate ai widget rimangono uguali. `app_shutdown()` distrugge il contesto
prima della GPU e della finestra. Il chiamante non deve distruggere separatamente
`app->ui`, che appartiene all'applicazione.

Per impostare il tema fuori dal loop, dopo un `app_init()` riuscito:

```c
UIContext *previous = ui_context_get_current();
if (ui_context_set_current(app->ui)) {
    ui_set_theme(ui_theme_light());
    ui_context_set_current(previous);
}
```

## Uso manuale

Le API sono dichiarate in `include/ui/ui.h`:

- `ui_context_create()`: alloca un contesto iniziale; restituisce NULL in caso
  di memoria insufficiente e non modifica la selezione corrente.
- `ui_context_get_current()`: restituisce il contesto selezionato, eventualmente NULL.
- `ui_context_set_current(ctx)`: seleziona il contesto; NULL deseleziona.
- `ui_context_reset(ctx)`: libera gli snapshot e ripristina tutto lo stato,
  inclusi tema, callback feedback, notifiche e diagnostica. Mantiene la selezione
  corrente e il collegamento al backend di rendering.
- `ui_context_destroy(ctx)`: libera il contesto e lo deseleziona se corrente.
  Accetta NULL; un puntatore distrutto non può essere riutilizzato.

Selezionare un contesto prima delle API UI che leggono o modificano stato: non
esiste un contesto predefinito implicito. Accoppiare `ui_begin_frame()` e
`ui_end_frame()`. Reset e distruzione di un contesto con frame aperto falliscono;
il cambio di selezione fallisce se il contesto uscente o entrante ha un frame
aperto. Selezionare nuovamente lo stesso contesto è un'operazione innocua.
`reset(NULL)` restituisce false. `app_shutdown()` durante un frame UI aperto
conserva le risorse e segnala `APP_ERROR_STATE`: completare il frame prima di
riprovare lo shutdown.

Il reset invalida gli stati interni conservati dai widget. Per cancellare solo
l'editing testuale del contesto selezionato resta disponibile `ui_text_input_reset()`.

## Confini attuali

La selezione è una sola, sul thread principale: queste API non sono thread-safe.
I pool dei widget hanno ancora capacità fisse; lo stato del
[picker opzionale](UI_FILE_PICKER.md) viene invece allocato solo al primo utilizzo.
L’adattatore APP usa ancora il collector di piattaforma condiviso. Il nuovo
[ingresso esplicito del frame](UI_FRAME_INPUT.md) accetta invece input e canvas
forniti dal chiamante, distinti per contesto. Il [backend di rendering](UI_RENDERER.md) è collegato per contesto, ma
l’adattatore Vulkan mantiene clipping e cache texture condivisi; creare più contesti non duplica né instrada
automaticamente gli eventi.
Il contratto applicativo resta una finestra attiva per volta.

Completare la preparazione e il submit della geometria prima di cambiare contesto.
Il cambio rilascia l'anteprima transiente del picker nel renderer condiviso;
il picker può ricaricarla al prossimo disegno. Reset e distruzione rilasciano
anch'essi l'anteprima posseduta. Le APP e le risorse grafiche prese in prestito
per disegnare un picker devono vivere più a lungo del suo contesto.
Le operazioni di disegno passano dal backend del contesto; non è ancora un
sistema multi-finestra nel renderer Vulkan.

## Verifiche

`make test` comprende il test `ui_context_test`: fallimento di allocazione,
isolamento di tema/ID/focus/dropdown/notifiche, snapshot lunghi, consumo input,
reset, protezioni durante i frame e rilascio delle anteprime tramite mock.
Il test del ciclo applicativo copre creazione fallita, shutdown/ripartenza,
rifiuto dello shutdown durante un frame e ripristino del contesto precedente.

Build debug, release e headless; suite ordinaria e ASan/UBSan:
[evidenze](../evidence/ui-context/). I test con mock non verificano il driver GPU.
Il rapporto runtime separato documenta la scena Vulkan e il probe input nativo.
LeakSanitizer, altri sistemi/driver e più finestre simultanee non sono certificati.
