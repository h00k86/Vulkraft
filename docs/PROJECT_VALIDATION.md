# Validazione e misure del framework

Questi comandi si riferiscono al checkout completo GRAPHICS_APP_BASE; la
distribuzione sorgente dell'SDK contiene un sottoinsieme con build CMake.

## Un comando ripetibile

```sh
make validate
```

Esegue in sequenza: confini architetturali, test debug e SDL dummy, build
debug/release delle demo e dell'editor SDL/Vulkan, suite ASan/UBSan, distribuzione
sorgente, build CMake isolate, test dell'SDK e consumatori esterni dopo lo
spostamento dell'installazione. Seguono dieci smoke SDL: basic, navigazione,
editing, layout e colore, ciascuno immediato e differito; infine i benchmark
dei pulsanti e dell'editor. Anche lo strumento di misura dell'editor viene
compilato ed eseguito con ASan/UBSan.
Non apre finestre e non invia input al desktop. Richiede le dipendenze di build
del progetto, SDL2/SDL2_image, FreeType, pkg-config, CMake e Python 3.

`evidence/consolidation/validation.json` registra comando, esito, durata e log
di ogni fase. Il processo termina con codice non zero al primo fallimento.
Il report viene aggiornato anche all'avvio delle fasi: un'interruzione lascia
visibile `RUNNING`, senza dichiarare un risultato finale riuscito.
I dettagli della verifica del package sono conservati in `sdk/`.

La validazione include inoltre gli stage `app-source` e `app-sdk`: prepara
l'archivio GraphicsApp 0.4.0 con lista esplicita, manifest e checksum, ne verifica
la riproducibilità e ricostruisce gli SDK GLFW/SDL2 Debug/Release esclusivamente
dalle sorgenti estratte. Controlla i 28 header pubblici, installazione spostata,
consumer esterni, distribuzione delle risorse senza link e workflow UI/controller
dell'editor con file reali e renderer deterministico. Report annidati in `app-sdk/`.
Gli archivi di questa esecuzione sono in `ui-source/` e `app-source/`, senza
sovrascrivere gli artefatti delle precedenti release in `build/`.

`--runtime` aggiunge gli smoke Vulkan del pacchetto estratto, senza inviare input
desktop. `--native` aggiunge sia questi smoke sia il workflow nativo dell'editor
esterno su entrambi gli host. Dettagli in
[GRAPHICS_APP_DISTRIBUTION.md](GRAPHICS_APP_DISTRIBUTION.md).

Per una CI o per conservare una sessione separata:

```sh
python3 tools/validate_project.py --jobs 4 --output-dir /tmp/ui-validation
```

Le build debug/release sono separate; eseguire una sola validazione alla volta
nello stesso checkout. Una nuova esecuzione nella stessa cartella delle
evidenze sostituisce i report della precedente.

## Input nativo

```sh
make validate-native
```

Ripete la validazione e aggiunge il flusso desktop su X11/i3 per entrambi gli
host. Richiede xdotool e Pillow, accesso alla sessione grafica e disponibilità
di mouse/tastiera durante la prova. Scrive solo documenti temporanei e sostituisce
la clipboard con testo sintetico. Ogni finestra viene identificata con PID e
titolo congiunti e verificata nuovamente per PID. Lo script interrompe l'input
se la finestra figlia perde inaspettatamente il focus.

La variante `--focus-cycle` apre un secondo editor di proprietà dello script,
controlla che il primo perda il focus del campo e torna al primo prima di
proseguire. Il resto copre click completi accodati, clipboard, cronologie locale
e del documento, picker, salvataggio, resize, annullamento della chiusura,
salvataggio prima della chiusura e riapertura in un nuovo processo. Vulkan
richiede inoltre validation layer attivo e log senza errori.

Per ripetere solo questa parte:

```sh
python3 tools/config_editor_native_validation.py --backend all --quick-clicks \
  --document-history --focus-cycle --output-dir evidence/consolidation/native
```

Questa è una prova di interazione automatizzata. Valutazione visiva manuale,
Wayland, altre GPU/OS e LeakSanitizer restano verifiche distinte.

## Benchmark CPU

```sh
make benchmark-ui
./build/ui-benchmark.out > /tmp/ui-benchmark.json
./build/ui-editor-benchmark.out > /tmp/ui-editor-benchmark.json
```

Usa il core release `-O2`, un renderer simulato con metriche di testo semplici
e 32, 128 o 512 pulsanti con ID stabili: 20 frame di riscaldamento e 200 frame
misurati per ogni percorso immediato/differito. Riporta media, mediana e p95
del tempo monotono; include begin/end, dichiarazione widget e replay differito.
Una sonda privata separata misura ricerche riuscite nel registro dei pulsanti,
con distribuzione uniforme sugli ID già presenti. Non espone nuove API pubbliche.

I wrapper GNU ld per malloc/calloc/realloc/free contano le richieste del codice
collegato staticamente. I byte del contesto derivano dalla sua creazione isolata;
i byte per frame sono richieste cumulative, non memoria residente o picco heap.
Non comprendono overhead dell'allocatore, memoria dei driver o allocazioni
interne alle librerie dinamiche. La capacità dei comandi e dell'arena dei testi
differiti è già stata riscaldata; le copie delle etichette rimangono incluse,
ma non richiedono nuove allocazioni se rientrano nella capacità raggiunta.

Il benchmark dei pulsanti non misura campi in editing o carichi misti.
Entrambi escludono rendering GPU, presentazione e rasterizzazione dei font.
Il clock e i wrapper aggiungono overhead.
Confrontare esecuzioni sulla stessa macchina; nessuna soglia temporale assoluta
blocca la CI. I risultati servono a decidere dove intervenire, non certificano
un framerate dell'applicazione.

## Carico misto dell'editor e memoria

`ui-editor-benchmark.out` usa direttamente `config_editor_draw()`, il controller
e il documento dell'esempio, con lo stesso backend simulato nei percorsi
immediato e differito. Ogni caso usa un contesto nuovo, 20 frame di
riscaldamento e 200 misurati su canvas 1000x800. Sono presenti sette casi per
percorso: inattivo con 1 o 32 profili; editing UTF-8/undo/redo/backspace su un
profilo; scroll con testi variabili su 32 profili; menu sovrapposto; picker
aperto; picker riaperto con scansione della directory a ogni frame.

Il menu è aggiunto dalla scena di misura: non è una funzione aggiunta all'editor.
Il picker usa 32 file temporanei, senza anteprime, preferiti o scritture nella
configurazione utente. Il caso di riapertura include I/O del filesystem con
cache del sistema operativo disponibile, non simula un disco freddo.
La verifica controlla ogni passaggio del ciclo di editing e lo spostamento
dello scroll, l'apertura effettiva di menu/picker e il numero di voci lette.
I conteggi delle callback di disegno devono coincidere tra i due percorsi.

Il JSON distingue setup con primo frame, riscaldamento e frame misurati.
Riporta byte richiesti ancora allocati e relativo massimo logico, richieste e
free per frame, cronologie di testo, picker, buffer dinamici dei dropdown e
memoria fissa dell'applicazione. Comprende inoltre riserva degli owner degli
eventi e payload dinamici delle voci dei menu.
La memoria della draw list viene ricavata per differenza dopo avere sottratto
contesto, applicazione, cronologie, snapshot, picker, opzioni dei dropdown,
owner degli eventi e voci dei menu
dal totale osservato.
Alla distruzione il totale delle allocazioni osservate deve tornare a zero;
questo controllo non sostituisce LeakSanitizer.

Il dettaglio per dominio usa sizeof dei singoli store privati; non importa il
layout fisico di UIContext. La somma viene confrontata con la richiesta reale
di creazione del contesto, conservando separatamente il padding. Per i registri
principali sono disponibili capacità, voci popolate/dichiarate e byte dei
relativi slot: una voce conservata non è necessariamente visibile nel frame.
La capacità libera è una riserva deliberata, non memoria automaticamente
eliminabile senza cambiare i contratti.

L'osservatore usa una tabella fissa di 8192 allocazioni: la sua dimensione è
riportata separatamente. Gli allocator builtin sono disabilitati nello
strumento e i contatori sono volatile, perché l'interposizione GNU ld avviene
dopo le ottimizzazioni del compilatore. Un controllo iniziale verifica
l'aggiornamento dei byte durante malloc/realloc/free. Il massimo riportato
esclude memoria temporanea interna a realloc, overhead dell'allocatore e
allocazioni interne alle librerie dinamiche, incluse quelle di opendir/libc.

Le sonde dei lookup per pulsanti e navigazione sono separate dal tempo dei
frame: misurano ricerche riuscite uniformi sui registri popolati. Non attribuiscono
una percentuale del frame alle ricerche; servirebbe un profiling distinto.
`make test-benchmark-sanitize` verifica soltanto il nuovo strumento e i moduli
che collega. `make validate` conserva i due JSON in `benchmark.json` e
`benchmark-editor.json` nella cartella delle evidenze scelta.

## Diagnostica e sessioni ripetute

Le suite `ui_error_test` e `ui_session_test` fanno parte di `make test`,
`make test-sanitize` e CTest su Linux; sono incluse nel pacchetto sorgente.
La seconda esegue 27.648 frame con tre seed, due contesti, 36 cicli di reset
e percorsi immediato/differito, verificando il ritorno della memoria osservata
al valore iniziale dopo ogni ciclo e a zero dopo ogni distruzione.
Le misure sono byte richiesti osservati, non RSS o memoria del driver.
Dettagli in [UI_ERRORS.md](UI_ERRORS.md). Evidenze aggiornate in
`evidence/ui-reliability/`.

La suite `config_editor_session_test` estende la verifica alla composizione
reale dell'editor e al filesystem temporaneo: 72 workflow concatenati,
22.608 frame, 72 scritture fallite indotte, 72 conflitti esterni e 90 riaperture
tramite picker o controller nuovo. La semantica dei due tipi di riapertura e
i limiti delle misure sono descritti in
[CONFIG_EDITOR_VALIDATION.md](CONFIG_EDITOR_VALIDATION.md).

## Versione di riferimento dell'SDK

La verifica del package comprende metadati normalizzati, rigenerazione
byte per byte dalla copia estratta, variazione dei timestamp/permessi dei
sorgenti, accordo tra versione CMake/header/core collegato e rifiuto delle
richieste di versioni differenti. Dodici test degli strumenti di release
controllano anche il rifiuto di rapporti incompleti/falliti, sorgenti o
archivi cambiati dopo la verifica e l'etichettatura delle prove native precedenti.
Sono eseguiti da Make e da CTest quando Python è disponibile.

Il report unificato contiene la versione. `finalize_ui_release.py` lo lega
all'hash dell'archivio verificato, controllando nuovamente i sorgenti rispetto
al manifest prima di scrivere il riepilogo. Dettagli in [UI_RELEASE.md](UI_RELEASE.md).
