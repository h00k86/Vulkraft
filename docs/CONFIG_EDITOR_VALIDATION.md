# Consolidamento dell'editor

## Prova nativa ripetibile su Linux/X11

```sh
make config-editor-sdl2 config-editor-vulkan
python3 tools/config_editor_native_validation.py
```

Per ripetere un solo backend usare `--backend sdl2` o `--backend vulkan`;
in questo caso scrive `report-sdl2.json` o `report-vulkan.json`.
`--output-dir` sceglie una cartella separata per le evidenze; `--focus-cycle`
aggiunge una perdita/recupero del focus tramite un secondo processo editor.
La verifica completa del repository è descritta in
[PROJECT_VALIDATION.md](PROJECT_VALIDATION.md).

Lo script richiede X11, i3, xdotool e Pillow. Apre soltanto processi figli
dell'editor e seleziona la loro finestra visibile per PID e titolo. Verifica il
focus prima di ogni input e interrompe la prova se lo perde. Durante il test
lasciare mouse e tastiera liberi; l'input resta automatizzato, non manuale.

Usa directory temporanee e sostituisce gli appunti con testo sintetico senza
leggere il contenuto precedente. La prova copre entrambi i backend:

- selezione del campo col mouse e digitazione;
- copia/incolla tramite clipboard nativa e undo/redo;
- salvataggio tramite picker e verifica del file scritto;
- ridimensionamento a 700×650 e ritorno a 1000×800;
- richiesta di chiusura tramite window manager, dialogo e annullamento;
- salvataggio prima della chiusura;
- nuovo processo che riapre il file, lo modifica e salva sul percorso corrente;
- uscita pulita e, per Vulkan, assenza di errori di validazione.

Report, log e schermate sono in `evidence/config-editor-native/`. Lo script
fallisce se uno dei controlli non riesce; il report conserva i passi completati.

## Correzione emersa durante il consolidamento

L'host SDL dell'editor ora imposta `SDL_HINT_FRAMEBUFFER_ACCELERATION=0`, come
la demo di riferimento. Anche la superficie di presentazione resta sul percorso
software; si evita la possibile creazione di una superficie accelerata con
sostituzione della finestra nativa durante l'avvio.

## Verifica manuale ancora distinta

La prova automatizzata non sostituisce una sessione d'uso prolungata. Restano da
valutare manualmente comfort della navigazione, testi lunghi, molti profili,
comportamento su altri window manager/Wayland, altre GPU e altri sistemi operativi.
Il percorso APP conserva ora le transizioni del pulsante sinistro in una coda
di 128 eventi, insieme alla posizione normalizzata al momento dell'evento.
Consegna una transizione per frame: un tap completo viene elaborato come
pressione e rilascio in due frame, senza perderlo né ripeterlo. Una raffica
introduce quindi latenza proporzionale agli eventi accodati. Perdita del focus
o saturazione annullano i gesti pendenti; la saturazione produce una diagnostica.
Il collector è per una sola finestra, come quelli di tastiera e scroll, e non
fornisce un ordinamento unico fra eventi mouse e tastiera.

La prova nativa standard mantiene premuto il pulsante per 120 ms. Per accodare
pressione e rilascio prima del polling di entrambi gli host dell'editor:

```sh
python3 tools/config_editor_native_validation.py --quick-clicks
```

Questa variante sospende brevemente solo il processo figlio durante il click e
lo riprende anche in caso di errore. Salva evidenze separate in
`evidence/config-editor-native-quick-clicks/`. I test headless verificano anche
due tap nello stesso batch, coordinate originali dopo movimento, rilascio fuori
dal controllo, perdita del focus, overflow e recupero.

Anche l'adattatore SDL conserva ora fino a 128 transizioni e ne consegna una
per frame, mantenendo le coordinate logiche originali. I test SDL dummy con
widget reali coprono due tap nello stesso batch, movimento successivo, duplicati,
rilascio fuori controllo, focus, overflow e recupero. Precisazione sulla diagnosi
precedente: gli host SDL di esempio già separavano gli eventi interrompendo il
polling dopo ogni transizione; il limite riguardava chi usava l'adattatore con
un batch completo. Ora la garanzia è nell'adattatore e gli host leggono tutti
gli eventi, inclusi focus e chiusura, senza fermarsi al primo click.

LeakSanitizer non è incluso nelle prove ASan/UBSan già eseguite.

La distribuzione sorgente `build/ui-framework-0.1.0-source.tar.gz` contiene
manifest SHA-256 ed esempi. `make test-ui-package` verifica build isolate,
installazione, spostamento dell'SDK e ricompilazione di consumatori esterni.
Non è una promessa di compatibilità ABI: ricompilare i consumatori con gli header
corrispondenti. La prova nativa dipende dall'ambiente desktop del repository e
non è un test automatico dell'SDK standalone.

## Sessioni prolungate della composizione reale

`config_editor_session_test` concatena 72 workflow su tre seed (`1`,
`0x12345678`, `0xc0ffee`) e sui due percorsi immediato/differito. La fixture
costruisce documenti da 1, 4 o 32 profili tramite l'API del documento; le
modifiche successive attraversano la vista reale con Tab, Enter ed eventi
di testo/tastiera, senza inviare input al desktop.

Ogni workflow include editing UTF-8, undo/redo locale e del documento,
scrittura parziale indotta con wrapper `fwrite`, annullamento di Nuovo/Chiudi,
undo/redo attraverso salvataggi, modifica esterna del file, conflitto annullato,
salvataggio di una copia e riapertura dal picker. Il file originale modificato
esternamente deve rimanere intatto. Ogni quarto workflow salva prima di chiudere
ed esegue una riapertura con controller nuovo, senza cronologia ereditata;
il blocco successivo usa anche un nuovo contesto UI e una nuova draw list.
La riapertura con controller nuovo è simulata nello stesso processo, non
costituisce la verifica nativa di avvio di un nuovo processo.

Ogni frame verifica clip bilanciate, assenza di errori UI, indici validi della
cronologia e flag `consumed` dell'input del chiamante invariato. Dopo ogni blocco di quattro
workflow la memoria osservata deve tornare a zero, senza free non attribuiti.
La directory temporanea deve essere priva di file `.tmp.` residui e viene
rimossa al termine. I file salvati vengono confrontati con documenti attesi,
compresi i salvataggi falliti e i conflitti.

La suite è inclusa in `make test`, `make test-sanitize` e CTest con picker
abilitato su Linux. Esecuzione mirata:

```sh
make build/config_editor_session_test build/config_editor_session_test_sanitize
./build/config_editor_session_test
ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=halt_on_error=1 ./build/config_editor_session_test_sanitize
```

L'osservatore riusa i wrapper GNU ld del benchmark. Conta le richieste collegate
staticamente, non RSS, overhead dell'allocatore o allocazioni interne delle
librerie dinamiche. La suite non verifica clipboard nativa, rendering visivo,
font reali, prestazioni GPU o LeakSanitizer. Risultati di questa campagna in
`evidence/editor-session/`.

## Ultima verifica nativa completa

Il 4 novembre 2026 la sequenza con `--backend all --quick-clicks
--document-history --focus-cycle` è passata su SDL2 e Vulkan, compreso il
nuovo processo che riapre il documento, lo modifica e salva. Report, log e
screenshot: `evidence/editor-session/native-confirmed-idle/`.
Tutti i controlli del report sono veri; i log sono puliti e il validation
layer Vulkan è attivo. I tentativi precedenti interrotti restano conservati.
Questo risultato non estende la verifica a sessioni manuali, Wayland,
altri OS/GPU o LeakSanitizer.
