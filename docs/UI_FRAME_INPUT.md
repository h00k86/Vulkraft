# Input esplicito del frame UI

`ui_begin_frame_input()` permette di iniziare un frame senza `APP`, finestra o
collector della tastiera. Riceve un `UIFrameInput` con dimensioni del canvas,
mouse, tempo trascorso, scroll, perdita di focus e batch ordinato degli eventi.
Il core UI non include più `application.h`, `platform/window.h` o
`input/keyboard.h` e non legge più le dimensioni globali del canvas.

## Chiamata diretta

```c
UIFrameInput frame = {
    .width = 1280, .height = 800,
    .mouse_x = mouse_x, .mouse_y = mouse_y,
    .mouse_down = mouse_down,
    .delta_time = delta_seconds,
    .scroll_delta = wheel_delta,
    .focus_lost = lost_focus,
    .events = events, .event_count = event_count
};
if (ui_begin_frame_input(&frame)) {
    /* Comporre i widget usando il renderer disponibile. */
    ui_end_frame();
}
```

Selezionare prima un `UIContext`. Il batch usa i tipi portabili in
`input/events.h` e i codici `PLATFORM_KEY_*`, non codici GLFW/SDL nativi.
Gli eventi vengono copiati durante la chiamata; il chiamante può riusare il
proprio array subito dopo. Il consumo modifica solo la copia nel contesto.
Il flag `consumed` in ingresso è conservato: un evento già consumato dal
chiamante non viene elaborato dai widget. Un batch vuoto fa scadere gli eventi
del frame precedente. La UI non esegue polling e non duplica i batch per altri
contesti: l'instradamento spetta al chiamante.

Coordinate, dimensioni del canvas e disegno devono usare la stessa unità logica.
Le dimensioni sono conservate per contesto; prima del primo frame valgono
1280×800. Il renderer corrente conserva ancora il proprio canvas globale:
l'adattatore APP garantisce che i due valori coincidano. Il percorso diretto
richiede che il chiamante mantenga questa corrispondenza quando disegna.

La funzione restituisce false, senza modificare il contesto, per:

- contesto assente o frame già aperto;
- puntatore al frame NULL, array NULL con conteggio diverso da zero o conteggio
  superiore a `INPUT_KEYBOARD_CAPACITY`;
- dimensioni non positive/non finite, coordinate o scroll non finiti, tempo
  negativo/non finito.

Un batch oltre capacità viene rifiutato interamente. Il produttore deve fornire
eventi validi e ordinati e un array lungo almeno quanto il conteggio dichiarato.
Il campo opzionale `debug_keys_down` alimenta la diagnostica `UI_INPUT_DEBUG`.

## Compatibilità APP

`ui_begin_frame(APP*, ...)` rimane disponibile con la firma precedente in `ui/app_adapter.h`.
La sua implementazione è ora in `src/ui/ui_app_adapter.c`: raccoglie gli eventi,
legge le dimensioni dell'applicazione e chiama il nuovo ingresso esplicito.
L'adattatore consuma anche `input_scroll_take()` e consegna la rotella nello stesso
batch di tastiera e focus: una perdita del focus annulla tutti questi input insieme.
Il ciclo APP non applica più lo scroll dopo l'inizio del frame. Nei loop personalizzati
registrare sia `input_keyboard_register()` sia `input_scroll_register()` e rimuovere
l'eventuale `ui_set_scroll_delta(input_scroll_take())` successivo a `ui_begin_frame()`:
altrimenti si sovrascriverebbe il delta già consegnato con zero. Le chiamate ai widget
in `app_draw()` non cambiano. L'API diretta `UIFrameInput` resta indipendente dai collector.

Il collector APP mantiene lo stato senza focus tra i frame: fino al successivo
evento di recupero, il batch rimane annullato, compresi mouse e scroll consegnati
dall'adattatore. Se perdita e recupero arrivano nello stesso polling, quel batch
è scartato e l'input riprende dal seguente. Le callback precedenti continuano a
ricevere gli eventi. Una nuova registrazione assume focus iniziale finché la
piattaforma non segnala diversamente; il collector resta a finestra singola.

Il collector resta a finestra singola, mentre chi usa l'ingresso diretto può
fornire batch distinti ai propri contesti. Questo non aggiunge da solo supporto
multi-finestra al renderer.

## Verifiche e limiti

`focus_lost` annulla le pressioni dei pulsanti e i drag di slider/scrollbar,
rilascia il focus degli editor e chiude i dropdown. Il batch di tastiera, la
rotella e lo stato premuto del mouse di quel frame vengono ignorati. Non viene
generato un click o un rilascio sintetico. L'host deve sopprimere l'input mentre
la finestra resta senza focus e notificare ogni perdita di focus al core.

`tests/ui_frame_input_test.c` compila il core senza definizione di APP, backend
di finestra, collector, globali del canvas o librerie GLFW/Vulkan. Verifica
inserimento UTF-8 ordinato, batch immutabile, scadenza eventi, focus, scroll,
dimensioni per contesto e rifiuto dei frame invalidi. Verifica anche pressione
seguita da perdita di focus (nessun click), click normale successivo, cancellazione
del drag e snapshot incoerente con tasti/mouse ancora attivi. Il disegno è simulato.
`tools/check_boundaries.py` impedisce di reintrodurre nel core le dipendenze
applicative e il polling rimossi.

Le evidenze di build debug/release/headless, test e runtime Vulkan sono in
[evidence/ui-frame-input](../evidence/ui-frame-input/).

Il disegno, la misura del testo, il clipping e le anteprime ora passano dal
[backend UI per contesto](UI_RENDERER.md). Font è ora opaco per la UI, con metriche fornite dal backend; il filesystem
del picker appartiene a [un modulo opzionale](UI_FILE_PICKER.md); i widget non ricevono più APP* (vedi [migrazione](UI_WIDGET_API.md)).
