# Editing del testo e clipboard

`ui_text_input()` e i widget che lo riusano supportano selezione da tastiera,
clipboard dell'host e cronologia per ID/contesto. Il core non include SDL, GLFW
né APP. Il risultato del widget rimane `true` solo alla conferma con Invio;
il buffer del chiamante viene modificato durante l'editing.

| Tasti | Comportamento |
| --- | --- |
| Shift + freccia sinistra/destra | Estende/riduce la selezione di uno scalare Unicode |
| Shift + Home/End | Seleziona fino all'inizio/fine |
| Ctrl+A | Seleziona tutto, senza cancellare |
| Frecce senza Shift | Chiudono la selezione sul bordo corrispondente |
| Digitazione, Backspace, Delete | Sostituiscono/eliminano la selezione |
| Ctrl+C / Ctrl+X / Ctrl+V | Copia / taglia / incolla |
| Ctrl+Z | Undo |
| Ctrl+Y oppure Ctrl+Shift+Z | Redo |
| Invio / Escape | Conferma / ripristina il testo all'ingresso in editing |

La selezione viene evidenziata; la vista orizzontale tiene visibile il cursore.
Click attiva il campo, ma posizionamento del cursore e selezione tramite mouse
non sono introdotti da questo intervento. `ui_float_field()` e i campi numerici
delle property row conservano il loro editor di conferma separato.

## Collegamento dell'host

`ui_context_set_clipboard(context, &callbacks, user)` copia una tabella
`UIClipboard`; `NULL` scollega il servizio. La sostituzione è permessa solo fra
frame. Reset conserva il collegamento; il chiamante mantiene vivo `user` fino
alla sostituzione/distruzione. Le callback sono sul thread principale e non
devono richiamare le API UI.

- `read(user, &text, &bytes)` restituisce un intervallo UTF-8 preso in prestito:
  non serve un NUL finale. `false` indica indisponibilità/errore.
- Dopo ogni lettura riuscita il core invoca `release(user, text)` esattamente
  una volta, se presente, anche se rifiuta il contenuto. La memoria deve restare
  valida fino a quel momento. Se `read` fallisce, pulizia a carico dell'host.
- `write(user, text)` riceve una stringa terminata, valida solo durante la
  chiamata. Deve copiarla se la conserva. `false` lascia intatto il testo durante
  il taglio. In assenza di selezione copia/taglia non cambiano la clipboard.

La mancanza delle callback rende le rispettive scorciatoie innocue; selezione,
digitazione e undo restano disponibili. Nessuna clipboard globale nascosta nel core.

APP collega automaticamente il servizio tramite `ui_bind_app_renderer()`;
con un renderer personalizzato chiamare `ui_bind_app_clipboard(app)`.
La finestra deve vivere più del collegamento. Il backend GLFW usa la clipboard
nativa; quello headless usa memoria privata per prove senza desktop.

SDL: chiamare `ui_sdl2_bind_clipboard(context)` dopo l'inizializzazione video.
L'adattatore libera con `SDL_free` ogni risultato di `SDL_GetClipboardText`.
Scollegare/distruggere il contesto prima di `SDL_Quit`. La demo SDL lo fa già.

## UTF-8, capacità e fallimenti

La selezione usa offset in byte riallineati ai confini degli scalari UTF-8.
L'incolla rifiuta l'intero intervallo se malformato o maggiore di 1 MiB; filtra
C0/C1 (inclusi NUL, tab, CR/LF) e separatori di riga U+2028/U+2029 per mantenere
il campo monoriga. Non normalizza né aggiunge spazi al posto dei separatori.

Se lo spazio non basta, inserisce il prefisso filtrato che entra interamente,
senza tagliare una sequenza UTF-8. Se non entra alcuno scalare, il testo e la
selezione restano intatti. Un carattere digitato che non entra non elimina la
selezione. Un errore di lettura, scrittura o allocazione per la clipboard non
provoca la perdita della selezione o del testo.

## Cronologia

Ogni campo conserva fino a **16 undo** (17 revisioni incluso il presente),
con un massimo di **64 KiB di testo allocato**. Le revisioni più vecchie vengono
eliminate per rispettare entrambi i limiti. Ogni snapshot può occupare al massimo
**32 KiB incluso NUL**: documenti maggiori restano editabili, ma azzerano la
cronologia. La memoria viene liberata a reset, distruzione o riuso dello slot.

Ogni evento che cambia il testo è un passo; un incolla è un unico passo.
Non viene raggruppata automaticamente una parola digitata. Una modifica dopo
undo elimina il ramo redo. Spostamenti e selezione non aggiungono passi, ma la
revisione conserva la selezione precedente alla modifica. Se il buffer viene
ridotto, undo non ripristina revisioni che non entrano interamente.

Una modifica esterna al testo invalida la cronologia e aggiorna lo snapshot di
Escape; il cursore resta riallineato. Escape azzera la cronologia della modifica
annullata. Perdita di focus conserva il testo già digitato e interrompe l'editing.
Se un'allocazione della cronologia fallisce, l'editing prosegue e la cronologia
viene azzerata: non vengono mantenuti passi parziali o riferimenti al buffer.

Grafemi, shaping, BiDi, composizione IME, scorciatoie Cmd su macOS e accessibilità
screen reader restano lavori distinti.

## Verifiche

```sh
make test test-ui-sdl2
make build/ui_text_edit_test_sanitize build/ui_text_edit_app_test_sanitize
ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=halt_on_error=1 ./build/ui_text_edit_test_sanitize
ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=halt_on_error=1 ./build/ui_text_edit_app_test_sanitize
make test-ui-sdl2-sanitize test-ui-package
make ui-reference-sdl2 ui-reference-vulkan
SDL_VIDEODRIVER=dummy ./build/ui-reference-sdl2-debug.out --editing-smoke
SDL_VIDEODRIVER=dummy ./build/ui-reference-sdl2-debug.out --editing-smoke --immediate
APP_REFERENCE_EDIT_SMOKE=1 ./build/ui-reference-vulkan-debug.out
```

I test core includono clipboard assente/fallita, UTF-8, limiti, undo/redo e
sostituzione esterna. SDL verifica la clipboard del driver dummy; APP attraversa
le callback della piattaforma headless e il collector. Le scene grafiche
verificano selezione/sostituzione/undo senza leggere o modificare la clipboard
del desktop. Non equivalgono a una prova manuale con tastiera o clipboard nativa.

## Modulo di editing interno

`src/ui/ui_text_edit.c` è una unità compilata separatamente. Il contratto privato
in `ui_text_edit_internal.h` definisce lo stato del campo e le operazioni su
selezione, cronologia e snapshot, insieme alla riconciliazione del buffer
sostituito dal chiamante. Lo stato continua a essere incorporato nel `UIContext`;
il modulo di editing non consulta il contesto né contiene operazioni di disegno.
La clipboard viene passata come tabella di callback e relativo `user`.

Il widget in `ui.c` gestisce ancora layout, focus, instradamento degli eventi,
conferma con Invio, regole della password e disegno. Le API pubbliche e i limiti
di cronologia conservano il comportamento precedente. In particolare il widget
blocca copia/taglia dei campi password prima di inoltrare l'evento all'editor.

`ui_text_edit_module_test` collega solo il sorgente del modulo, senza il resto
del core. Verifica selezione UTF-8, clipboard e cronologie indipendenti,
modifiche esterne, annullamento, limiti di undo e buffer non validi. Su Linux
inietta anche fallimenti di malloc per snapshot, copia/taglia, incolla e
cronologia: una lettura clipboard riuscita viene rilasciata anche se l'allocazione
fallisce. L'iniezione usa il linker solo nel test e non aggiunge API allocatore
al framework. Il test è incluso in Make/CTest e nel pacchetto; il target
`build/ui_text_edit_module_test_sanitize` abilita ASan/UBSan.
