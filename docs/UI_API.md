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
il tema da solo. Non include alpha, area HSV o contagocce dello schermo.

I canali visualizzati usano la conversione già adottata da `ui_color_srgb`
(gamma 2.2 approssimata); il colore del renderer resta lineare. Una modifica
quantizza a 8 bit per canale. Il semplice disegno non modifica colori validi né
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
