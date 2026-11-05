# Esempio completo: editor di configurazione

L'applicazione gestisce fino a 32 profili con nome e descrizione UTF-8, stato
abilitato e livello 0–100. Elenco, proprietà, picker e dialoghi sono la stessa
composizione UI nei due eseguibili; cambiano solo finestra, input e rendering.

## Avvio

Dalla radice del repository:

```sh
make config-editor-sdl2
./build/config-editor-sdl2-debug.out

make config-editor-vulkan
./build/config-editor-vulkan-debug.out

# Aprire il documento di esempio
./build/config-editor-sdl2-debug.out --open examples/config_editor/example.cfgapp
```

`--root directory` limita la navigazione del picker e i percorsi accettati
alla directory scelta (default: directory corrente). `--open file` apre un file
all'avvio; il percorso è relativo alla directory corrente, non a `--root`.
Un errore iniziale viene scritto sul terminale e restituisce exit code 1.

SDL supporta anche `--immediate`, `--font percorso.ttf` e `--dump file.bmp`.
Senza `--immediate` usa la draw list. Per un SDK installato, passare il font
installato in `share/ui-framework/fonts/OpenSans-Italic.ttf`.
Vulkan usa le risorse del progetto tramite il normale adattatore APP.

## Flusso di lavoro

- **Nuovo** crea un documento con un profilo iniziale.
- **Apri** usa il picker filtrato per `.cfgapp` e valida tutto il documento
  prima di sostituire quello corrente.
- **Salva** usa il percorso corrente; senza percorso apre il picker.
- **Salva come** chiede il percorso e conferma la sovrascrittura di file esistenti.
- **Chiudi**, compresa la X della finestra, chiede come gestire le modifiche
  non salvate: **Salva e continua**, **Continua senza salvare**, **Annulla**.
  Lo stesso dialogo precede Nuovo e Apri quando il documento è modificato.

Annullare un picker, un dialogo o la conferma di sovrascrittura conserva il
contenuto corrente. Anche dopo “continua senza salvare”, Apri conserva il vecchio
documento finché il nuovo file non è stato caricato con successo. Un errore di
salvataggio lascia il documento modificato e interrompe l'azione sospesa.
Mentre un picker o una conferma è aperto, ulteriori richieste di chiusura della
finestra sono ignorate: completare o annullare il dialogo attivo.

Nome e descrizione sono modificati direttamente tramite `ui_text_input()`:
selezione, clipboard, undo/redo e Tab sono quelli del framework. Il nome è
obbligatorio. Il livello usa pulsanti -10/+10, senza un valore numerico in attesa
di conferma. Aggiunta/rimozione dei profili e le altre proprietà non hanno undo
applicativo; la cronologia è quella dei soli campi di testo.

Il confronto con il documento dell'ultimo caricamento/salvataggio determina lo
stato modificato. Cambiare selezione o aprire un dialogo non sporca il documento.
Il documento iniziale è un modello pulito senza percorso: salvarlo per conservarlo.

### Modifiche esterne

Prima di salvare sul percorso corrente, l'editor rilegge il file e confronta i
dati con l'ultimo documento caricato/salvato. Se differiscono, oppure il file è
stato rimosso, è malformato o non può essere letto, interrompe il salvataggio.
Il dialogo offre **Salva una copia in un altro file** e **Annulla**, conservando
sia il documento locale sia quello su disco. “Salva come” sul medesimo percorso
applica lo stesso controllo. Un percorso non risolvibile produce invece un
errore senza scrivere.

La copia riuscita riprende un eventuale Nuovo/Apri/Chiudi sospeso; annullare il
dialogo o il picker interrompe anche quell'azione. Per caricare la versione
esterna, annullare e usare Apri, confermando lo scarto delle modifiche locali.

Il confronto è semantico: controlla profili, ID e proprietà, senza basarsi su
timestamp o dimensioni. Cambiare solo LF/CRLF non è un conflitto. Non è un lock:
un'altra scrittura fra controllo e sostituzione rimane possibile. Il controllo
riguarda il file corrente; per una destinazione diversa valgono la conferma di
sovrascrittura del picker e gli stessi limiti di concorrenza.

## Separazione del codice

| File in `examples/config_editor/` | Responsabilità |
| --- | --- |
| `document.h/.c` | Modello, validazione e formato; nessun widget o backend |
| `editor.h/.c` | Documento corrente/salvato, selezione, percorso e azioni sospese |
| `view.h/.c` | Composizione UI condivisa e integrazione del picker |
| `sdl_main.c` | Host SDL, draw list opzionale e gestione della chiusura |
| `vulkan_main.c` | Host APP/Vulkan e osservatore degli eventi di chiusura |

Gli ID dei campi includono identità del profilo e generazione del documento:
aprire un altro file non riusa focus o undo del documento precedente.
Il core UI non conosce il formato né apre file. La scrittura atomica è nel
modulo filesystem, `platform_write_file_atomic()`; il modulo picker dell'SDK
include questo servizio e il relativo header pubblico.

## Formato `.cfgapp`, versione 1

File UTF-8 con campi separati da TAB e righe terminate da LF (lettura anche CRLF):

```text
CONFIG_EDITOR<TAB>1<TAB>prossimo_id
id<TAB>abilitato<TAB>livello<TAB>nome<TAB>descrizione
```

`<TAB>` indica un tab reale, non questi cinque caratteri. Nome massimo 95 byte,
descrizione massimo 255 byte; non sono ammessi TAB, controlli o separatori di
riga nei testi. ID positivi univoci, inferiori al prossimo ID; abilitato 0/1,
livello intero 0–100. Lettura limitata a 64 KiB, senza troncamenti silenziosi.
File malformati, versioni sconosciute, UTF-8 invalido o troppi profili vengono
rifiutati senza sostituire documento, percorso o baseline salvata.

Il salvataggio scrive un temporaneo esclusivo nella stessa directory, verifica
scrittura/flush/chiusura e poi sostituisce la destinazione. Errori precedenti
alla sostituzione non troncano il file esistente. La directory deve consentire
creazione e sostituzione di file; su Unix è questo permesso a governare la
sostituzione atomica, non soltanto il bit di scrittura del file.

Il servizio rifiuta destinazioni che siano link o file non regolari; l'editor
risolve prima i percorsi canonici entro la root. Non implementa locking,
monitoraggio continuo dei file, autosalvataggio o recupero da crash. Non promette
durabilità della directory in caso di perdita di alimentazione, né protezione
contro modifiche concorrenti ostili al filesystem. Windows ha un ramo dedicato
nel servizio filesystem, ma questa consegna è verificata su Linux.

## Verifiche ripetibili

```sh
make test-config-editor test-config-editor-sanitize
SDL_VIDEODRIVER=dummy ./build/config-editor-sdl2-debug.out --smoke
SDL_VIDEODRIVER=dummy ./build/config-editor-sdl2-debug.out --smoke --immediate
./build/config-editor-vulkan-debug.out --smoke
make test-ui-package
```

`--smoke` usa un documento nuovo, digita UTF-8, richiede la chiusura e annulla
il dialogo. Termina dopo 30 frame su SDL e 31 su Vulkan; non salva documenti,
non modifica la clipboard e non invia tasti al desktop. L'host SDL riceve un
`SDL_WINDOWEVENT_CLOSE` nella propria coda; la prova Vulkan chiama l'osservatore
registrato e alimenta le callback input. Non è una prova manuale della X nativa.

Il test del modello verifica round-trip, parser, percorsi non accessibili e
transizioni salva/scarta/annulla. Su Linux inietta una scrittura parziale per
verificare l'integrità del file precedente. Il test UI percorre con Tab/Invio
modifica, salvataggio tramite picker, Nuovo, apertura tramite picker, annullamento
dei dialoghi e salvataggio prima della chiusura. I sanitizzatori usano ASan/UBSan
con LeakSanitizer disabilitato. CMake verifica anche i due percorsi SDL e il
consumatore esterno ricompilato da un SDK installato e spostato.

I test dei conflitti includono modifica esterna, rimozione e file malformato,
tentativo di Salva come sul percorso corrente, annullamento, copia riuscita e
chiusura sospesa. Il test UI percorre anche il dialogo e il picker della copia.

La prova nativa di consolidamento è descritta in
[CONFIG_EDITOR_VALIDATION.md](CONFIG_EDITOR_VALIDATION.md).

## Cronologia del documento

I comandi **Annulla modifica** e **Ripristina modifica**, sotto le proprietà,
agiscono sull'intero documento: profili, nomi, descrizioni, abilitazione e livelli.
Sono raggiungibili con mouse o Tab/Invio. Ctrl+Z/Ctrl+Y restano riservati alla
cronologia locale del campo di testo attivo.

La cronologia conserva fino a 32 operazioni tramite snapshot in memoria, senza
allocazioni durante la modifica. Digitazioni consecutive nello stesso campo
sono raggruppate fino all'uscita dal campo o a un'altra operazione. Undo/redo
ripristinano anche selezione e identificatori dei profili, e rigenerano gli ID UI
per evitare il riuso di una vecchia cronologia locale del testo.

Salvare non cancella la cronologia: lo stato delle modifiche dipende sempre dal
confronto con il documento salvato. Nuovo e Apri riuscito la azzerano; errori e
annullamento del picker la conservano. Una modifica dopo undo elimina il ramo
redo. Durante picker, conferme, conflitti o chiusura completata i comandi non
agiscono. La cronologia non viene scritta nel file e non recupera sessioni dopo
un crash. Anche valori temporaneamente non salvabili, come un nome vuoto,
possono essere annullati.

La prova nativa opzionale comprende undo, salvataggio, redo e nuovo salvataggio:
`python3 tools/config_editor_native_validation.py --document-history`.
Le evidenze sono separate in `evidence/config-editor-document-history/`.
