# File picker opzionale

Il file picker risiede in `src/ui/ui_file_picker.c`, separato dal core dei widget.
Il suo header pubblico è `ui/file_picker.h`; `ui/ui.h` continua a includerlo per
compatibilità degli include. `ui_file_picker_draw()` non riceve più APP*: vedi
la [migrazione delle firme](UI_WIDGET_API.md).

## Collegamento

```sh
make ui-standalone    # build/libui_debug.a: soltanto core e draw list
make ui-picker        # aggiunge build/libui_picker_debug.a
```

Un consumatore che usa il picker collega le librerie in questo ordine:

```sh
cc -Iinclude my_ui.c build/libui_picker_debug.a build/libui_debug.a -lm -o my_ui
```

Con `BUILD=release` vengono prodotte le varianti `libui_picker_release.a` e
`libui_release.a`. La libreria applicativa completa continua a includere il
picker automaticamente. Le librerie UI non richiedono GLFW o Vulkan.

`libui_picker` comprende il widget e gli adattatori `paths.c`/`platform_fs.c`.
`libui` contiene solo `ui.c` e `ui_draw_list.c`: non include quei moduli e non
ha riferimenti alle loro funzioni o alle operazioni di lettura dei file.
Il link dell'esempio standalone non usa eliminazione delle sezioni inutilizzate.

## Stato e ciclo di vita

Il picker alloca il proprio stato al primo tentativo di apertura. Ogni contesto
possiede un'istanza distinta, conservata per riuso dopo l'annullamento. Il core
mantiene solo un puntatore opaco allo stato e le informazioni di cattura input
necessarie ai widget e alla diagnostica; non invoca funzioni del modulo opzionale.

- `ui_file_picker_open()` configura root, filtro, modalità e preferenze come prima.
  In caso di allocazione fallita il picker resta chiuso: verificarlo con
  `ui_file_picker_is_open()`.
- `ui_file_picker_cancel()` chiude anche la conferma di sovrascrittura e rilascia
  la cattura dell'input. È valida anche prima della prima apertura.
- Reset e distruzione del contesto liberano lo stato opzionale e l'eventuale
  anteprima, mantenendo le protezioni contro un frame UI aperto.
- Il cambio di contesto conserva la navigazione ma rilascia l'anteprima secondo
  il contratto del backend. Il picker del nuovo contesto rimane indipendente.

Le funzioni filesystem, i controlli di contenimento, la navigazione e la
persistenza di preferiti/recenti rimangono nel modulo. Confermare un percorso
non legge o scrive il contenuto del file: questa operazione spetta all'app.
La palette può salvare preferenze nella directory utente quando richiesta.

## Verifica

`tests/ui_file_picker_test.c` usa API pubbliche, callback simulate e una directory
temporanea. Core e picker sono compilati in unità separate; il test ordinario
collega le due librerie. Copre fallimento di allocazione, selezione cartella,
salvataggio di un nuovo percorso, conferma di sovrascrittura, rifiuto di `..`
fuori dalla root, annullamento, diagnostica, reset e isolamento fra contesti.

`make test-ui-picker` e `make test-ui-picker-sanitize` eseguono questi controlli;
sono inclusi anche nelle suite generali. Il controllo architetturale impedisce
il ritorno di dipendenze filesystem nel core. Le evidenze in
[evidence/ui-picker-module](../evidence/ui-picker-module/) includono anche
archivi, simboli e grafo degli include del consumatore standalone.

La scena Vulkan verifica l'integrazione applicativa; il test del picker usa
input sintetico e disegno simulato. Non certifica l'interazione manuale completa
del picker sul driver grafico, altri sistemi operativi o LeakSanitizer.

Il backend può omettere il servizio anteprime: la selezione del file resta
funzionante e il pannello mostra il messaggio di immagine non caricabile.
Vedi [anteprime opzionali](UI_OPTIONAL_PREVIEWS.md).
