# UI Framework API

Questa versione identifica un insieme verificato di header e sorgenti. Non viene
promessa compatibilità ABI tra versioni differenti: ricompilare i consumatori.
Il package CMake accetta richieste di versione esatta.

- `ui/version.h`: versione degli header e del core collegato, senza contesto.
- `ui/ui.h`: contesto, input esplicito, widget, layout e clipping.
- `ui/renderer.h`: nove callback obbligatorie; decorazioni e anteprime opzionali.
- `ui/draw_list.h`: registrazione differita, primo errore conservato, replay unico;
  `ui_draw_list_discard()` abbandona la lista senza iniziare un nuovo frame.
- `ui/file_picker.h`: servizio opzionale filesystem, da collegare separatamente.
- `text/font_types.h`: font opaco, posseduto dal backend.
- `backends/ui_sdl2.h`: implementazione SDL2 opzionale con font FreeType proprio.

Il core non dipende da SDL, GLFW, Vulkan o APP. L'adattatore APP/Vulkan del progetto
originale non è incluso nel package UI standalone. Il picker resta incluso negli
header per compatibilità, ma le sue funzioni richiedono il target picker.

Tab/Shift+Tab, attivazione dei controlli e focus modale sono descritti in
[UI_KEYBOARD_NAVIGATION.md](UI_KEYBOARD_NAVIGATION.md). `ui_keyboard_focus_id()`
espone l'ID corrente; il focus è incluso in `ui_has_keyboard_capture()`.

Selezione, clipboard dell’host e undo/redo: [UI_TEXT_EDITING.md](UI_TEXT_EDITING.md).
`ui_context_set_clipboard()` collega callback opzionali per contesto.

Tutte le operazioni UI avvengono sul thread principale con contesto selezionato.
Un frame inizia con `ui_begin_frame_input`, compone widget e popup, termina con
`ui_end_frame`. Contesto, font, backend e immagini devono rispettare i rispettivi
vincoli di durata. Completare/scartare il lavoro del renderer prima di sostituire
risorse, iniziare una nuova draw list o distruggerla. Non fare reentry da callback.

Le unità di editing UTF-8 sono scalari Unicode; non ci sono shaping, gestione dei
cluster o composizione IME. Il nuovo backend non amplia queste garanzie.

## Color picker e accento del tema

`ui_property_color(layout, id, font, label, &color)` compone un selettore RGB
inline: otto preset, tre slider con campo numerico (0–255), codice `#RRGGBB`
di sola lettura e anteprima. Occupa sei righe del property layout, eredita
focus/modalità dalla UI e supporta Tab, frecce e Home/End tramite i controlli
esistenti. Il valore `UIColor` appartiene all'applicazione; il widget non cambia
il tema da solo. Le varianti estese aggiungono RGBA e area HSV; non c’è
un contagocce dello schermo.

La variante estesa aggiunge presentazione intera e inizio del trascinamento:

```c
UIColorPickerOptions options = { .integer_channels = true };
UIColor before = color;
bool drag_started = false;
bool changed = ui_property_color_ex(&layout, "color", font, "Colore", &color,
                                    &options, &drag_started);
/* Per undo: conservare before quando drag_started è true; continuare lo stesso
 * gesto nei frame seguenti. changed segnala anche preset e modifiche da tastiera. */
```

`out_drag_started` è opzionale e viene sempre inizializzato a false, anche se
l'input non è valido. Diventa true soltanto nel primo frame di un trascinamento
RGB con il puntatore, anche se il valore non cambia. In HSV/RGBA il segnale
comprende anche il riquadro SV, H/S/V e alfa. Restano false i clic sui
preset, le modifiche nel campo numerico e i comandi da tastiera. Non occorre
conoscere gli ID dei widget figli. Snapshot e raggruppamento undo appartengono
all'host: il valore precedente va copiato **prima** della chiamata.

`integer_channels` arrotonda solo la presentazione a riposo dei canali sRGB
0–255. Per esempio `137.006` appare `137`; aprendo il campo si ritrova il testo
preciso. Disegnare o confermare senza modifiche non quantizza un colore valido.
Con opzioni NULL o azzerate rimane la presentazione decimale precedente.
Le opzioni sono lette durante la chiamata, senza trattenerne il puntatore.

I canali visualizzati usano la conversione già adottata da `ui_color_srgb`
(gamma 2.2 approssimata); il colore del renderer resta lineare. Una modifica
in modalità RGB quantizza a 8 bit per canale; HSV mantiene i float. Il semplice disegno non modifica colori validi né
ne riduce la precisione; componenti non finite o fuori intervallo vengono
normalizzate. Le istanze devono avere ID distinti, anche dentro scope ripetuti.

Nella demo condivisa SDL/Vulkan, **Opzioni aspetto** in fondo al pannello apre
il picker per il colore di accento. Focus, tab attiva e sfondi di selezione si
aggiornano; il colore personalizzato resta attivo passando al tema chiaro/scuro.
**Ripristina colore predefinito** ripristina accento e selezione del tema corrente.
Il colore è conservato solo per la sessione, senza scrivere preferenze su disco.

Per vedere direttamente il dialogo e verificarne la selezione da tastiera:

```sh
./build/ui-reference-sdl2-debug.out --color-smoke
APP_REFERENCE_COLOR_SMOKE=1 ./build/ui-reference-vulkan-debug.out
```

Queste prove selezionano il preset verde e terminano dopo circa 30 frame.
`make test-ui-color test-ui-color-sanitize` verifica conversione, preset, input
RGB, normalizzazione, cambio di tema e ripristino (ASan/UBSan, senza LSan).

## Radio, avanzamento, tooltip e password

La demo condivisa offre **Widget basilari** in fondo al pannello, vicino a
Opzioni aspetto. Il dialogo mostra i quattro controlli. Per la prova automatica:

```sh
./build/ui-reference-sdl2-debug.out --basic-smoke
APP_REFERENCE_BASIC_SMOKE=1 ./build/ui-reference-vulkan-debug.out
```

- `ui_radio_button`: più istanze condividono un `int selected`, ciascuna con
  un valore `option` diverso. Il chiamante inizializza la selezione. Mouse al
  rilascio, Tab e Invio/Spazio; le opzioni disabilitate non prendono focus.
- `ui_progress_bar`: frazione 0–1 limitata all'intervallo; modalità indeterminata
  con fase temporale fornita dal chiamante. Non riceve input. Label opzionale,
  altrimenti percentuale nella modalità determinata.
- `ui_tooltip`: chiamare dopo il controllo con lo stesso ID di navigazione e il
  suo rettangolo. Hover o focus da tastiera per il ritardo richiesto, poi disegno
  a fine frame. Non prende focus né intercetta input. Una riga, troncata alla
  larghezza dello schermo, fino a 511 byte UTF-8; una proposta per frame. Il
  puntatore Font resta valido fino a `ui_end_frame`. Menù e modali hanno priorità.
- `ui_property_password`: una riga con etichetta, editing UTF-8 e Mostra/Nascondi.
  Il chiamante conserva buffer e `bool revealed`, inizialmente false. Usa un
  asterisco per punto di codice (non per grafema). Il renderer riceve soltanto
  la maschera quando nascosta, anche per misure e posizione del cursore.
  Selezione, incolla, undo/redo ed Escape riusano l'editing standard; copia e
  taglia sono disabilitati anche in modalità visibile. Il risultato indica
  conferma con Invio; il buffer cambia durante l'editing.

Il campo password non è un sistema di autenticazione o un archivio sicuro:
buffer e snapshot di editing restano in chiaro in memoria, senza garanzie di
azzeramento sicuro. La demo non salva la password su disco e non la stampa.

`ui_basic_widgets_test` verifica scelta esclusiva, esclusione dei disabilitati,
limiti della progress bar, ritardo del tooltip, mascheratura e modifica UTF-8,
mostra/nascondi e blocco copia/taglia. È incluso in `make test` e CTest; la variante
`ui_basic_widgets_test_sanitize` usa ASan/UBSan con LSan disabilitato.

## Politica comune di interazione

Il core usa `ui_pointer_hit` come punto comune per hit testing, vincoli dei
modali e clip aperti tramite `ui_clip_push`/scope panel e scroll. I widget
precedenti e quelli nuovi rispettano quindi gli stessi rettangoli visibili.
Il clip applicato direttamente dal renderer resta solo grafico: il layout
applicativo deve usare gli scope UI per esprimere anche la regione interattiva.

`ui_interaction_internal.h` raccoglie tre operazioni private: acquisizione su
pressione, attivazione al rilascio e trascinamento. Sono condivise da pulsanti,
checkbox, radio, slider, splitter e scrollbar; le tab riusano l'acquisizione
sulla pressione. Editing, menù e dropdown mantengono le proprie sequenze di
eventi, utilizzando l'hit testing comune.

- Entrare in un controllo con il pulsante già premuto altrove non avvia un gesto.
- Pulsanti/checkbox/radio si attivano solo dopo pressione e rilascio nello
  stesso controllo; uscire e rientrare durante la pressione è consentito.
- Un trascinamento già acquisito può continuare fuori dalla maniglia e dal clip.
- Disabilitazione, perdita del focus e blocco modale annullano il gesto.
- Pulsanti e slider che spariscono per almeno un frame perdono il gesto prima
  di un eventuale riuso dello stesso ID.

Il cambiamento non introduce una coda di eventi nuova, una proprietà globale
esclusiva del puntatore o una nuova API pubblica. Il refactoring conserva
UIContext e backend esistenti e separa la politica del gesto dal disegno.
Le sovrapposizioni arbitrarie di widget non sono risolte automaticamente tramite
z-order: gli overlay continuano a usare le regole di modali e popup del core.

`ui_interaction_test` verifica le stesse sequenze su pulsanti, checkbox e radio,
più regressioni dello slider precedente; fa parte di Make/CTest ed è disponibile
con ASan/UBSan. La politica di attivazione su pressione resta intenzionale per
tab e campi di testo, diversamente dall'attivazione al rilascio dei pulsanti.

## Moduli interni compilati separatamente

Il core comprende unità C autonome, tutte incluse in `ui_core` (CMake) e
`libui` (Make):

- `ui.c`: proprietà del contesto, ciclo del frame e facciata di disegno.
- `ui_buttons.c`: pulsanti, checkbox/toggle, radio e tooltip.
- `ui_numeric_controls.c`: campi numerici, slider, tab e splitter.
- `ui_text_field.c`: widget del campo testo/password, focus e presentazione.
- `ui_capture.c`: cattura di click/tastiera/rotella, Escape, gesti e diagnostiche.
- `ui_menu.c`: menu e sottomenu.
- `ui_dropdown.c`: dropdown e pass di disegno dei popup.
- `ui_modals.c`: ciclo di vita e disegno dei dialoghi modali.
- `ui_layout.c`: righe, colonne, spaziature e layout annidati/property.
- `ui_properties.c`: composizione delle property row, inclusa la password.
- `ui_panels.c`: sezioni, pannelli, scope di scroll e relative diagnostiche.
- `ui_text_edit.c`: selezione UTF-8, clipboard, cronologia, snapshot Escape e
  riconciliazione del buffer; riceve stato del campo e servizio clipboard.
- `ui_navigation.c`: ordine del focus, proprietà degli eventi e ripristino modale;
  opera sul contesto ricevuto esplicitamente, senza disegnare.
- `ui_theme.c`: conversione dei colori e costruzione dei tre temi, senza contesto.
- `ui_color_picker.c`: composizione del color picker tramite API dei widget.
- `ui_progress.c`: disegno della barra di avanzamento.
- `ui_draw_list.c`: registrazione e riproduzione dei comandi di disegno.

`ui_paint_internal.h` espone ai moduli solo le operazioni di disegno necessarie;
la risoluzione del contesto corrente e del renderer resta in `ui.c`.
`ui_geometry_internal.h` contiene la validazione condivisa dei rettangoli.
Questi header sono privati e non fanno parte degli header installati dell'SDK.
Le firme pubbliche e la gestione del tema per contesto restano invariate.

`ui_widget_internal.h` dichiara i servizi condivisi per hit testing, pressioni,
drag e campi numerici/testo. Le property row compongono questi servizi e le API
pubbliche senza accedere alla struttura del contesto. I pannelli conservano i
propri registri nel contesto e riusano la politica di input del core; lo stato
non è duplicato nei moduli.

I moduli non includono la definizione fisica di `UIContext`: accedono a strutture
private dedicate tramite gli header `ui_state_*_internal.h`. Proprietà, limiti e
contratti sono descritti in [UI_INTERNAL_ARCHITECTURE.md](UI_INTERNAL_ARCHITECTURE.md).
Le regole degli overlay sono descritte in [UI_INPUT_CAPTURE.md](UI_INPUT_CAPTURE.md).

Undici suite prima legate all'inclusione di `ui.c` ora collegano
la libreria; i test di contratto possono usare una struttura privata dedicata
senza conoscere il layout del contesto. Restano alcune verifiche legacy che
includono il core per simulare errori o ispezionare lo stato complessivo.

## Diagnostica e affidabilità

`ui_begin_frame_input_status()` restituisce la causa del rifiuto senza modificare
il contesto. `ui_context_error()` conserva il primo errore dei punti coperti
fino a clear/reset. Codici, precedenza, copertura e limiti in
[UI_ERRORS.md](UI_ERRORS.md). Le API booleane esistenti restano disponibili.

## Versione e aggiornamenti

La versione corrente proviene da `ui/version.h`. `UI_VERSION_STRING` descrive
gli header e `ui_version_string()` il core collegato. Entrambi sono accessibili
senza inizializzare contesto o backend. CMake espone `UIFramework_VERSION`.
Regole di aggiornamento, distinzione API pubbliche/private e compatibilità
sono in [UI_COMPATIBILITY.md](UI_COMPATIBILITY.md); generazione del pacchetto
e associazione delle verifiche in [UI_RELEASE.md](UI_RELEASE.md).

## Segnaposto nei campi

`ui_text_input_ex` aggiunge `UITextInputOptions` senza cambiare `ui_text_input`:

```c
UITextInputOptions options = { .placeholder = "Cerca nella scena..." };
ui_text_input_ex("search", font, ANCHOR_TOP_LEFT, 0, 14, x, y, width,
                 search, sizeof(search), &options);
```

Il suggerimento appare nel colore muted quando il buffer è vuoto, anche con
il cursore attivo. È ritagliato al campo e non entra nel buffer, nella
clipboard, negli snapshot o nella cronologia undo. Lo stile opzionale resta
`options.style`. Stringhe e opzioni sono prese in prestito soltanto per la
chiamata. Password e API precedenti conservano il loro comportamento.

## Bottoni con icona

```c
UIIcon icon = { .kind = UI_ICON_LOCK };
UIButtonStyle style = { .selected = locked };
if (ui_icon_button("lock", font, bounds, &icon, NULL, &style)) locked = !locked;
ui_tooltip("lock", font, bounds, "Blocca il layer", 0.4f);
```

Camera, occhio, lucchetto aperto/chiuso, cartella, file, più, meno, chiusura,
play, pausa, sposta, ruota e scala sono sagome geometriche: non richiedono glifi né asset aggiuntivi.
Un'etichetta facoltativa appare accanto all'icona. I bottoni supportano
mouse, Tab/Invio/Spazio, selected e disabled; si disegnano nel rettangolo dato.
Le metriche min/max dello stile non ridimensionano quel rettangolo.
Per un'icona senza testo `font` può essere NULL; serve per etichetta e tooltip.

`UI_ICON_TEXTURE` usa `texture_slot` e UV `u0,v0,u1,v1` compresi in [0,1],
con ampiezza positiva. L'host possiede texture e durata delle risorse; il widget
non carica file. Le texture conservano i propri colori, mentre sagome e testo
usano il colore dello stile/tema. UV non validi vengono ignorati senza click.

## Lista e albero

```c
UIListItem items[] = {{"box", "Box", false}, {"light", "Luce", false}};
UICollectionResult result = ui_list("objects", font, viewport, items, 2, &selected);
if (result.activated_index >= 0) { /* Aprire l'elemento scelto. */ }

UITreeNode nodes[] = {
    {"scene", "Scena", 0, true, false},
    {"box", "Box", 1, false, false},
    {"light", "Luce", 1, false, false}
};
result = ui_tree("scene", font, viewport, nodes, 3, &selected_node);
```

Array, stringhe, selezione ed espansione appartengono all'host e devono essere
stabili tra le chiamate. Esempi locali: in un'applicazione conservare selezione
e `nodes[].expanded` nel modello, evitando di reinizializzarli ogni frame.
Le liste non trattengono puntatori agli array. Gli ID delle righe devono essere
univoci nella vista; dopo ordinamenti o rimozioni rimappare la selezione
tramite ID. `selected == -1` indica nessuna selezione.

L'albero usa preorder con prima profondità zero, massimo 64 e incrementi di
al massimo uno. I figli sono dedotti dalla profondità successiva; un ramo
chiuso nasconde tutti i discendenti. La selezione di un discendente nascosto
ritorna al ramo visibile. Un input non valido viene ignorato; una vista vuota
accetta array NULL e svuota la selezione.

Ogni vista occupa un solo stop Tab. Su/Giù e Home/End selezionano righe
abilitate, Invio/Spazio restituiscono `activated_index`. Nell'albero Destra
espande o entra nei figli, Sinistra chiude o risale. Il clic su una riga la
seleziona; il clic sul simbolo +/- espande senza attivare la riga. I risultati
separano `selection_changed` ed `expansion_changed`. `clicked_index` segnala il
clic primario completato sul corpo, anche se la riga era già selezionata; resta
-1 per tastiera, toggle di espansione e bottoni accessori.

La rotella e la scrollbar usano il contesto UI; la selezione da tastiera viene
portata nel viewport. Vengono disegnate solo le righe visibili, con clipping e
abbreviazione UTF-8. L'attraversamento del modello rimane O(n) per frame;
non è un indice virtuale per milioni di nodi. Questa prima API usa selezione
singola; multiselezione e drag/reparenting non sono inclusi.

### Accessori dell'albero

```c
UITreeAccessory actions[] = {
    { .node_index = 0, .id = "visible", .icon = { .kind = UI_ICON_EYE },
      .selected = visible, .tooltip = "Visibile" },
    { .node_index = 0, .id = "lock", .icon = { .kind = UI_ICON_LOCK },
      .selected = locked, .tooltip = "Bloccato" }
};
UITreeOptions options = { .accessories = actions, .accessory_count = 2 };
UICollectionResult result = ui_tree_ex("scene", font, viewport, nodes, count,
                                       &selected_node, &options);
if (result.accessory_index == 0) visible = !visible;
if (result.accessory_index == 1) locked = !locked;
```

L'array descrive le azioni per tutte le righe con `node_index`; i bottoni sono
allineati a destra nell'ordine dell'array. `accessory_width=0` usa l'altezza della
riga; un valore positivo sceglie la larghezza. Le azioni si restringono su righe
molto strette. Il testo riserva lo spazio dei bottoni e viene troncato, senza
sovrapposizioni. Etichetta e tooltip sono opzionali, selected e disabled hanno
lo stesso significato dei normali bottoni con icona.

Un clic o Invio/Spazio su un accessorio restituisce l'indice dell'array in
`accessory_index`; tutti gli indici non attivati sono -1. Non cambia la selezione
e non attiva o espande il nodo. Le azioni sono gestite dall'host dopo la chiamata,
senza callback durante l'attraversamento. La riga disabled disabilita tutti i
suoi bottoni. Le azioni di righe nascoste o fuori viewport non ricevono input.
I bottoni visibili sono stop Tab aggiuntivi; gli ID devono essere univoci per
nodo e stabili tra frame. I puntatori sono presi in prestito solo per la chiamata.
Le azioni sono facoltative: `ui_tree` conserva il comportamento precedente.

## RGBA e HSV (SDR)

`UIColor` resta RGB lineare. Per l'alfa usare il nuovo `UIColorRGBA`, con alfa
straight in [0,1], senza conversione gamma sul canale A:

```c
UIColorPickerOptions options = { .mode = UI_COLOR_HSV, .integer_channels = true };
bool drag_started = false;
ui_property_color_rgba(&layout, "tint", font, "Tinta", &rgba, &options, &drag_started);
```

`mode` zero significa RGB, come prima. La variante RGB conserva le sei righe
originali; RGBA aggiunge la riga Alpha, l'hex `#RRGGBBAA` e un'anteprima su
scacchiera. Modificare solo alfa conserva esattamente RGB, e viceversa.

HSV aggiunge un riquadro per saturazione/valore, una striscia di tonalità e i
campi H/S/V. H usa gradi [0,360), S/V [0,1], dopo la conversione gamma già usata
dal framework. Il riquadro risponde al mouse e alle frecce (passo .01);
Home sceglie S=0,V=1, End S=1,V=1. Si può scorrere fuori dal riquadro durante
un trascinamento: i valori sono limitati ai bordi. Per RGB senza alfa usare
`ui_property_color_ex` con `.mode=UI_COLOR_HSV`.

A riposo H usa un decimale e S/V tre. L'alfa usa tre decimali per default;
con `integer_channels=true` usa una forma compatta (`1`, `0.6`, `0.625`),
conservando le frazioni. I campi attivi conservano il testo float preciso. Questa presentazione non arrotonda il modello.

La tonalità dei grigi e la saturazione del nero vengono ricordate per ID nel
contesto; reset/distruzione le eliminano. La cache usa 64 slot riutilizzabili,
senza allocazioni; esaurimento nello stesso frame segnala `UI_ERROR_LIMIT`
con `UI_OPERATION_COLOR_PICKER`. I registri condivisi di slider/campi numerici
restano gli stessi del framework e possono imporre un limite inferiore.

`ui_color_to_hsv` e `ui_color_from_hsv` sono conversioni indipendenti dal contesto.
Le varianti SDR normalizzano componenti non finite o fuori [0,1]; H finito
viene avvolto. Non usare questi controlli per preservare intensità HDR.
HDR ed eyedropper restano lavori distinti. I picker HSV non hanno una ruota:
la selezione bidimensionale è il riquadro SV.

La demo SDL/Vulkan offre **Componenti e colore RGBA/HSV**, in fondo al pannello:
la prima scheda mostra segnaposto, icone, lista e albero; la seconda RGB/HSV e
alfa. Nessuna modifica viene salvata su disco. Le regressioni automatiche sono
in `tests/ui_components_test.c` e nella suite del color picker.
