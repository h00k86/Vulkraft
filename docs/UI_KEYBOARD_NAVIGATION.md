# Navigazione da tastiera

La navigazione è nel core UI e usa gli stessi eventi portabili di `UIFrameInput`.
Non richiede GLFW, SDL o un renderer specifico. Il focus appartiene al contesto
corrente e si può leggere con `ui_keyboard_focus_id()`; la stringa vuota indica
assenza di un controllo selezionato.

## Controlli

| Controllo | Tasti |
| --- | --- |
| Tutti i controlli registrati | Tab / Shift+Tab: successivo / precedente, con ritorno all'inizio/fine |
| Pulsante, checkbox, toggle | Invio, Invio del tastierino o Spazio: attivazione; il repeat non ripete il click |
| Campo di testo | Digitazione e frecce dell'editor esistente; Invio conferma, Escape ripristina lo snapshot fuori dalle modali; Tab conserva il testo e passa oltre |
| Campo numerico | Invio valida e applica, Escape annulla; Tab abbandona il valore ancora non confermato |
| Slider | Frecce: un centesimo dell'intervallo; Home/End: minimo/massimo; il campo numerico esatto è una tappa distinta |
| Dropdown | Invio/Spazio apre; frecce scorrono il candidato; Home/End selezionano il primo/ultimo candidato; Invio/Spazio conferma; Escape o Tab chiude senza applicare il candidato |
| Dialogo modale | Tab resta nel dialogo attivo; Escape chiude il dialogo attivo anche durante l'editing; dropdown/menu aperti hanno priorità |

L'indicatore di focus usa il colore accent del tema e compare durante la
navigazione da tastiera. Movimento, click o rotella del mouse lo nascondono
senza eliminare il controllo selezionato. Autofocus e ritorno da una modale
conservano il metodo di input attivo. Un click seleziona anche il
controllo per il successivo Tab; un click sullo sfondo rilascia il focus. La
perdita del focus della finestra annulla navigazione, editor e gesti pendenti.
`ui_has_keyboard_capture()` include il controllo selezionato dalla navigazione.

## Ordine, ID e visibilità

L'ordine è quello di dichiarazione dei widget, con ID stabili e univoci. Le voci
disabilitate sono saltate; quelle non dichiarate nel frame scompaiono dall'ordine.
Quando il controllo selezionato scompare o viene disabilitato, il focus passa al
successivo ancora presente, oppure al primo disponibile. Eventi destinati al
controllo rimosso non attivano automaticamente il suo sostituto.

Come le altre strutture immediate-mode, l'ordine precedente è disponibile dopo
il primo frame di layout. Un Tab nel primissimo frame viene risolto alla fine
di quel frame; le azioni richiedono che il controllo sia stato dichiarato.
Il registro è limitato a 512 controlli per contesto/frame. Gli ulteriori controlli
non entrano nell'ordine di navigazione; non si promette navigazione di liste
virtualizzate i cui elementi non vengono dichiarati.

I controlli dentro `ui_scroll_begin` vengono portati in vista durante la
navigazione da tastiera, usando la geometria del frame precedente. La rotella
restituisce il controllo dello scroll al mouse. Per il clipping generico o un
layout personalizzato il chiamante resta responsabile della visibilità.

Un modale salva il focus del chiamante, mette a fuoco il primo controllo interno
(normalmente X) e lo ripristina alla chiusura. I modali annidati mantengono una
catena di ritorno. Rimane obbligatorio il corretto abbinamento begin/end e l'uso
di ID distinti. La scomparsa di un modale viene riconciliata a fine frame.

## Ordine degli eventi

Il routing conserva il destinatario di ogni evento: nella sequenza
`carattere → Tab → carattere` i due caratteri raggiungono i rispettivi campi,
anche con Shift+Tab verso un campo dichiarato prima dell'altro. I widget consumano
una copia degli eventi; il batch del chiamante rimane immutato.

Le copie degli ID destinatari vengono riservate in base al numero reale di
eventi, con crescita geometrica fino a `INPUT_KEYBOARD_CAPACITY`; non esiste
più un array di 256 KiB incorporato in ogni contesto. Gli ID restano completi,
senza hash o token: cambia la proprietà della memoria, non il routing.
La capacità viene riusata fino al reset/destroy. Se la riserva fallisce,
`ui_begin_frame_input()` restituisce false prima di modificare il frame o
consumare gli eventi; il chiamante può riprovare. Un batch ignorato per perdita
del focus non richiede memoria di routing.

Selezione del testo, clipboard e undo/redo sono descritti in
[UI_TEXT_EDITING.md](UI_TEXT_EDITING.md). Non sono inclusi composizione IME,
shaping o supporto screen reader. La gerarchia di menu contestuali
ha un contratto distinto e non viene estesa da questo intervento.

## Verifiche ripetibili

```sh
make test
make build/ui_navigation_batch_test
./build/ui_navigation_batch_test
make build/ui_navigation_test_sanitize
ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=halt_on_error=1 ./build/ui_navigation_test_sanitize
make ui-reference-sdl2 ui-reference-vulkan
SDL_VIDEODRIVER=dummy ./build/ui-reference-sdl2-debug.out --navigation-smoke
SDL_VIDEODRIVER=dummy ./build/ui-reference-sdl2-debug.out --navigation-smoke --immediate
APP_REFERENCE_NAV_SMOKE=1 ./build/ui-reference-vulkan-debug.out
make test-ui-package
```

La scena condivisa verifica testo, checkbox, slider, dropdown, tema, contatore,
apertura/chiusura del dialogo e del file picker e ritorno del focus. SDL usa batch espliciti; il
test Vulkan alimenta le callback registrate per il frame successivo e percorre
il collector APP. Nessuno dei due invia tasti al desktop. CTest esegue il test
core e, con i moduli necessari, entrambi i percorsi SDL immediato/differito.

## Organizzazione interna

`src/ui/ui_navigation.c` viene compilato separatamente e dichiara il proprio
contratto privato in `ui_navigation_internal.h`. Ogni operazione riceve un
`UIContext *` esplicito: il modulo non seleziona né consulta il contesto corrente
e non chiama renderer. Il core gli passa anche lo stato di cattura del menu
all'inizio del frame. Il bordo visivo del focus resta in `ui.c`.

Il modulo gestisce dichiarazioni del frame precedente/corrente, assegnazione
ordinata degli eventi, Tab/Shift+Tab, attivazione da tastiera, salvataggio e
ripristino modale e rilascio del focus degli editor con i relativi snapshot.
La memoria dello stato continua a essere posseduta dal `UIContext`; non è stato
introdotto un secondo registro né modificato il contratto pubblico dei widget.

`ui_navigation_module_test` collega la libreria normalmente, senza includere
`ui.c`. Verifica due contesti senza contesto corrente e senza renderer:
proprietà degli eventi prima/dopo Tab, consumo singolo dell'attivazione,
cattura del menu, cancellazione e rilascio degli snapshot del solo contesto
richiesto. È incluso in Make, CTest e nel pacchetto SDK; Make dispone anche
del target `build/ui_navigation_module_test_sanitize` con ASan/UBSan.

Esc nella modale segnala l’annullamento al chiamante, che gestisce i dati
del dialogo: non ripristina automaticamente buffer già modificati. I contratti
di integrazione sono riassunti in [UI_INTEGRATION_BEHAVIOR.md](UI_INTEGRATION_BEHAVIOR.md).
