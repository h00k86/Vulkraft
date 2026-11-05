# Analisi del codice — 26 ottobre 2026

**Aggiornamento successivo:** corretti i tre difetti ad alta priorità e
implementati gli interventi sui sette punti medi. Vedi
[`PRIORITY_HIGH_FIXES.md`](PRIORITY_HIGH_FIXES.md) e
[`PRIORITY_MEDIUM_FIXES.md`](PRIORITY_MEDIUM_FIXES.md) per verifiche e limiti
residui (in particolare Unicode/IME e gestione errori durante il rendering).
Il testo e i log originari sotto descrivono lo stato precedente alle correzioni.
Il runner richiama ora le regressioni permanenti e salva log `current-*`.

La base ha una separazione utile fra applicazione, UI, piattaforma e Vulkan,
ma richiede correzioni prima di essere usata come fondazione affidabile per
applicazioni diverse dalla demo. Sono stati riprodotti due errori di memoria
CPU e un errore nella gestione del fallimento del reload delle texture.
Build e test esistenti passano anche in presenza di questi difetti.

Non sono stati modificati sorgenti applicativi, header, shader o Makefile.
Sono stati aggiunti questo report e i materiali diagnostici in
[`evidence/code-audit/`](../evidence/code-audit/).

## Verifiche e limiti

| Verifica eseguita in questa analisi | Risultato |
| --- | --- |
| Ricompilazione completa debug, `make -B -j4` | PASS, nessun warning/error nel log |
| Build release, `make BUILD=release -j4` | PASS |
| `make test`, incluso checker architetturale | PASS |
| Probe ID annidati, ASan + UBSan | Errore riprodotto: global-buffer-overflow |
| Probe cursore campo testo, ASan + UBSan | Errore riprodotto: negative-size-param |
| Probe reload texture, codice reale con alcune API Vulkan simulate | Successo erroneo e 4 descriptor write con vista distrutta |
| Probe helper tastiera, percorsi picker e font | Difetti riprodotti |
| Clang Static Analyzer su 8 file selezionati; GCC analyzer sul font | Ulteriori segnalazioni sul percorso di lettura font |
| Nuovo avvio finestra/Vulkan, resize e interazione reale | NON ESEGUITO |
| LeakSanitizer, Windows/macOS, matrice di GPU/driver | NON VERIFICATI |

I precedenti log runtime in `docs/VALIDATION.md` documentano l'estrazione:
non sono nuove verifiche di questa analisi. I probe sono headless; nei due
probe UI ASan è attivo e LeakSanitizer è disabilitato esplicitamente.
Il test texture verifica il flusso CPU e i descriptor write intercettati,
non esegue rendering né valida un driver Vulkan.

Per riprodurre, dalla root:

```sh
make
python3 evidence/code-audit/run.py
```

Lo script attuale esegue `make test` e `make test-sanitize`, con aspettative
di comportamento corretto. I singoli probe e i loro log originari sono storici.

## Problemi prioritari

### 1. Alta — scrittura fuori buffer negli ID UI annidati

Riferimento: `src/ui/ui.c:70`, funzione `ui_scoped_id`.

`snprintf` restituisce la lunghezza che avrebbe scritto senza troncamento.
Il codice la somma a `used` anche quando supera lo spazio rimasto. La scrittura
successiva usa quindi un puntatore oltre il buffer e `256 - used` può
diventare una dimensione enorme per underflow di `size_t`.

Cinque scope da 60 caratteri sono sufficienti. Ripetendo la generazione per
attraversare il ring di 16 buffer, ASan rileva `global-buffer-overflow` alla
linea 79. Il danno fra buffer adiacenti può iniziare prima della segnalazione.
Evidenza: [`scope.log`](../evidence/code-audit/scope.log).

Correzione: verificare il risultato di ogni formattazione contro lo spazio
disponibile e propagare l'errore; preferibile un'identità numerica stabile
composta dagli scope, senza concatenazioni in buffer di dimensioni diverse.

### 2. Alta — cursore non riallineato al contenuto modificato dall'app

Riferimento: `src/ui/ui.c:2753`, in particolare `2850–2856`.

Il buffer del campo testo appartiene al chiamante, mentre il cursore vive
nello stato interno. Se il chiamante accorcia il testo mentre il campo ha
il focus, il cursore può restare oltre `strlen(buffer)`. Backspace converte
`len - caret + 1`, negativo, in una dimensione enorme per `memmove`.

Il probe usa un testo di un carattere e un cursore rimasto a 20: ASan segnala
`negative-size-param` con dimensione -18 alla linea 2854.
Evidenza: [`caret.log`](../evidence/code-audit/caret.log).

Correzione: prima di ogni elaborazione limitare il cursore alla lunghezza
effettiva, validare terminazione/capacità del buffer e documentare il contratto
di sincronizzazione fra chiamante e widget.

### 3. Alta — reload fallito riutilizza handle texture distrutti

Riferimenti: `src/render/texture_manager.c:149`,
`src/render/vulkan_texture.c:330` e `:439`.

Il reload distrugge la texture precedente prima di caricare quella nuova.
`destroy_texture` non azzera gli handle; quando `stbi_load` fallisce,
`load_texture_internal` azzera solo larghezza e altezza. Il controllo
successivo vede ancora view/sampler non nulli, dichiara successo e aggiorna
i descriptor con handle già distrutti.

Il probe collega loader, distruttore e aggiornamento descriptor reali, simula
le chiamate Vulkan interessate e usa un file non immagine esistente per
provocare il fallimento: successo=1, slot valido=1, quattro scritture con
la vecchia view. Evidenza:
[`texture-reload.log`](../evidence/code-audit/texture-reload.log).

Correzione: caricare una texture temporanea, sostituire la precedente solo
dopo successo, conservare la vecchia su errore e azzerare sempre le strutture
distrutte. Conservare anche modalità sampler e mipmap durante il reload:
attualmente una texture tiled viene ricaricata tramite il loader CLAMP.

### 4. Media — capacità della superficie non negoziate

Riferimento: `src/render/vulkan_swapchain.c:56`.

Formato BGRA sRGB e colorspace sono fissi; vengono imposti TRANSFER_SRC,
trasformazione IDENTITY e compositing OPAQUE. Il controllo di idoneità GPU
verifica che esistano formati, ma non che quello scelto sia disponibile.
Su una superficie con capacità diverse la creazione può fallire o violare
i requisiti Vulkan, anche senza avere richiesto una cattura del frame.

La specifica richiede una coppia formato/colorspace annunciata e flag
usage/transform/alpha supportati. Vedi
[VkSwapchainCreateInfoKHR, requisiti di validità](https://docs.vulkan.org/refpages/latest/refpages/source/VkSwapchainCreateInfoKHR.html).
Questa è una lacuna verificata nel codice; il guasto su una superficie
incompatibile non è stato riprodotto con un driver reale.

Correzione: scegliere dai formati disponibili, usare flag supportati e
rendere la cattura facoltativa. Se il formato può cambiare alla ricreazione,
ricreare anche il render pass compatibile, attualmente conservato.

### 5. Media — parser font dipendente dalla formattazione JSON

Riferimento: `src/render/font.c:27`.

Il parser cerca stringhe e usa `sscanf` con punteggiatura e spaziatura rigide.
Un JSON valido con spazi dopo `:` e prima di `{` può non popolare i bounds.
Le variabili locali `left/bottom/right/top` non sono inizializzate e il numero
di conversioni non viene controllato: usarle dopo una conversione fallita
introduce comportamento indefinito. La funzione restituisce comunque 1.

Nel probe il font locale produce u1=0.829102; un piccolo JSON valido con
spazi restituisce successo ma u1=0 nella prova eseguita. Quel valore non è
garantito, proprio perché il percorso usa variabili non inizializzate.
Evidenze: [`font-pretty.log`](../evidence/code-audit/font-pretty.log),
[`font-project.log`](../evidence/code-audit/font-project.log).

Non sono controllati neppure `fseek`, `ftell`, `malloc` e la lettura completa.
Clang segnala il caso `ftell == -1`; GCC segnala il possibile puntatore nullo.
Usare un parser JSON con validazione dei campi e caricamento atomico del font.

### 6. Media — stato tastiera aggiornato solo durante il focus

Riferimenti: `src/ui/ui.c:1738`, `:1806`, `:2828`.

`ui_float_key_pressed` e i lock statici del campo testo osservano i rilasci
solo quando il widget viene elaborato con focus. Dopo Enter il campo perde
il focus: rilasciare Enter fuori dal campo lascia il lock attivo. Alla
successiva attivazione la prima pressione può essere ignorata.
Il probe dell'helper restituisce 1 per la prima pressione e 0 per la successiva
con rilascio avvenuto senza chiamata all'helper.
Evidenza: [`keys.log`](../evidence/code-audit/keys.log).

Inoltre l'input ricostruisce testo dai tasti fisici: lettere/digit fissi,
meno convertito in underscore, assenza di caratteri accentati, repeat e
clipboard. Il renderer stesso legge un byte alla volta e non decodifica UTF-8.
Sono limiti documentati in parte, ma incidono direttamente sull'obiettivo di
una base per app generiche. GLFW distingue eventi di tasto e caratteri e
fornisce una callback Unicode per questi ultimi:
[guida ufficiale input GLFW](https://www.glfw.org/docs/latest/input_guide.html#input_char).

Correzione: raccogliere una volta per frame eventi, press/release/repeat,
testo Unicode e modificatori, poi distribuirli al widget che possiede il focus.

### 7. Media — controllo root del picker solo lessicale

Riferimento: `src/ui/ui.c:4072`.

`/tmp/base/../outside.txt` passa il controllo per prefisso pur risolvendosi
fuori dalla root; anche i link simbolici richiedono una politica esplicita.
Inoltre root `/` non riconosce `/tmp` come contenuto, e una root terminata
da slash può compromettere il controllo del separatore.
Evidenza: [`picker.log`](../evidence/code-audit/picker.log).

Questo dimostra un errore nel contratto del picker, non una scrittura esterna
effettuata dalla demo: il picker restituisce percorsi al chiamante.
Normalizzare root e current, validare initial_directory e risolvere i percorsi
esistenti; in salvataggio verificare il genitore canonico. Se il confine root
è solo una comodità di navigazione, documentarne comunque il comportamento.

### 8. Media — inizializzazione senza stato di errore coerente

Riferimenti: `src/render/gfx.c:28`, `src/render/init_vulkan.c:26`,
`src/render/vulkan_command.c:11`, `src/render/vulkan_sync.c:8`,
`src/render/vulkan_texture.c:328` e `:373`.

`gfx_init` può ritornare dopo un fallimento di allocazione; `app_init`
prosegue senza poterlo sapere. Un fallimento dell'allocazione GPU lascia
anche il contesto già allocato. Altrove gli errori chiamano direttamente
`exit`/`abort`, mentre diverse creazioni Vulkan non verificano il risultato:
command pool/buffer, sync iniziale, sampler e mapping staging texture.
Il mapping texture fallito può arrivare a `memcpy` con puntatore non valido.

Il PNG obbligatorio del font usa il loader tollerante ai file mancanti ma
l'inizializzazione non controlla l'esito prima di creare i descriptor.
Lo shutdown assume un'inizializzazione completa e dereferenzia `renderer.gpu`:
non offre un percorso generale per rollback o seconda chiamata.

Correzione: restituire un risultato strutturato, controllare ogni operazione
fallibile e rendere il cleanup valido dopo ogni fase di inizializzazione.

### 9. Media — identità UI più lunghe dei buffer di stato

Riferimenti: `src/ui/ui.c:623`, `:1711`, `:2677`, `:3075`.

Gli ID composti arrivano a 255 caratteri, ma numerosi stati ne conservano
127. Il lookup confronta l'ID completo con quello troncato: un ID lungo
non ritrova il proprio stato e può riallocarlo a ogni frame, perdendo focus,
apertura o animazione e consumando il pool. Anche i singoli scope sono
troncati a 63 caratteri.

Questo difetto è distinto dall'overflow del punto 1: rimane anche correggendo
il calcolo della capacità. Uniformare il contratto e rifiutare gli ID troppo
lunghi oppure usare ID numerici composti, gestendo le collisioni.

### 10. Media — campo intero implementato tramite float

Riferimento: `src/ui/ui.c:2502`.

`ui_property_int` passa valore e limiti attraverso `float` e usa il campo
float anche per l'editing. Gli interi oltre 2^24 non sono tutti rappresentabili
esattamente; il testo usa inoltre sei cifre significative. Con `INT_MAX` il
limite convertito diventa 2147483648, e un valore confermato a quel limite può
causare una conversione float→int fuori intervallo.

Il campo intero deve mantenere il testo e fare parsing come intero. Esiste già
`ui_parse_int_clamped`, ma questo percorso non la usa. Analisi statica del
flusso; non è stato riprodotto tramite interazione reale.

## Architettura e possibilità di riuso

| Pattern presente | Vantaggio | Limite concreto |
| --- | --- | --- |
| Moduli per strato e backend con puntatori opachi | UI/app non manipolano handle Vulkan; checker passa | I moduli backend dipendono quasi tutti da APP, che funge da contenitore generale |
| UI immediata con cache interna degli stati | Composizione semplice, layout e overlay già disponibili | Stato implicito, pool fissi e lookup lineari; circa 4.667 righe in un solo file |
| Batching della geometria UI e scissor per batch | Riduce submit/draw rispetto a un renderer per widget | Tutta la geometria è ricostruita; il numero di batch dipende dai cambi di clip |
| Buffer staging/device separati per frame | Evita sovrascritture fra frame in volo | Più copie e memoria; serve misurare sul carico reale |
| Semafori di fine rendering per immagine swapchain | Scelta corretta per distinguere presentazione e frame slot | Non prova da sola l'assenza di tutti i problemi di sincronizzazione |
| Libreria statica e `app_draw(APP*)` fornita dal client | La demo non è incorporata nella libreria | Hook unico, nessun user-data dedicato o ciclo update separato |

La gerarchia dichiarata nel README è sostanzialmente rispettata. Il valore
del progetto è soprattutto un'app desktop con UI 2D, testo MSDF, forme e
immagini già funzionanti. Per canvas complessi, rendering 3D, più finestre
o integrazione in un host esistente sono necessarie estensioni del contratto.

UI, texture manager, geometria testo, dimensioni virtuali, cache finestra,
rotella, preferenze V-Sync e diagnostica usano stato globale. Due APP non
sono indipendenti: gli slot texture possono appartenere al device dell'altra
istanza, e lo shutdown di una finestra chiama `glfwTerminate` per tutte.
Anche una riapertura nello stesso processo deve affrontare gli stati UI non
azzerati dal normale shutdown. Rendere esplicito il vincolo a una sola APP
oppure spostare lo stato nei rispettivi contesti prima di promettere multiwindow.

`VK_CTX` mantiene anche un fallback statico diverso per translation unit:
può nascondere un contesto mancante invece di fallire vicino all'origine.
Il commento in `gfx.c` descrive già un precedente guasto causato da questa
scelta; oggi il fallback rimane e rende meno evidente un uso scorretto.

## Prestazioni, portabilità e manutenzione

- Il loop ridisegna anche quando l'app desktop è inattiva. V-Sync limita il
  ritmo, ma rimangono composizione UI, upload e rendering. Prevedere invalidazione
  e attese eventi con timeout per animazioni quando il prodotto lo richiede.
- Upload texture e modifiche descriptor fermano queue/device. Il batch evita
  parte dei wait sui descriptor, ma il loader conserva submit sincroni per
  transizioni e copie. File picker, decode immagini e accesso filesystem
  avvengono sul thread UI: possibili blocchi con immagini o directory grandi.
- Ogni `texture_manager_load_tiled` richiama `ensure_repeat` anche su una texture
  già corretta: wait device, distruzione/ricreazione sampler e descriptor write.
  Memorizzare il sampler desiderato ed evitare lavoro se non cambia.
- La cache texture usa soltanto il path come chiave: una richiesta REPEAT
  modifica il sampler condiviso con chi aveva richiesto CLAMP. Separare
  risorsa immagine e sampler, o includere le opzioni nell'identità.
- Il tetto effettivo è 14 immagini normali, oltre font e preview. È coerente
  con README e shader; resta un limite importante per un'app ricca di icone.
- Lookup degli stati UI è lineare: il filtro hash riduce gli `strcmp`,
  non rende la ricerca O(1). Evitare conclusioni di prestazione tratte dal
  commento iniziale; profilare prima di cambiare struttura dati.
- La selezione GPU richiede anche COMPUTE nella queue graphics/present,
  benché questo renderer non usi compute: vincolo aggiuntivo da giustificare.
- La build debug usa nomi di flag Vulkan molto recenti nella stampa delle
  queue, incluso `VK_QUEUE_DATA_GRAPH_BIT_ARM`. Non è dichiarata una versione
  minima degli header SDK; SDK più vecchi possono non compilare quel codice.
- Il filesystem ha rami Windows/macOS, ma build/link sono Linux e
  l'inizializzazione instance/device non negozia la portability enumeration
  e subset necessarie negli ambienti Vulkan che le richiedono. I percorsi
  preferenze sono XDG e il riconoscimento dei path assoluti shader controlla
  soltanto `/`. Portabilità da completare e verificare su ciascun target.
- Al resize vengono ricreati pipeline e shader, pur usando viewport/scissor
  dinamici. Se il formato rimane uguale è lavoro evitabile, da misurare.
- Diversi commenti rimandano a mondo 3D, acqua, particelle ed editor rimossi,
  talvolta con spiegazioni storiche non più applicabili. Non sono dipendenze
  runtime, ma ostacolano la comprensione del codice corrente.

## Copertura e ordine consigliato degli interventi

L'unico test C incluso verifica parti dello stato UI tramite inclusione diretta
di `ui.c`; non esercita il percorso completo di input e disegno. Mancano
regressioni per i casi riprodotti e per inizializzazione parziale, fallimenti
di caricamento, capacità superficie, gestione risorse e ricreazione ripetuta.
Il checker verifica regole testuali di dipendenza: PASS non significa assenza
di problemi architetturali o correttezza Vulkan.

1. Correggere overflow ID, cursore e sostituzione texture; trasformare i probe
   corrispondenti in test che richiedono il comportamento corretto.
2. Uniformare errori/cleanup, parser font e negoziazione superficie.
3. Centralizzare eventi input, chiarire identità e limiti UI, correggere picker
   e campo intero.
4. Definire il contratto di prodotto: singola o più finestre, callback del
   client, UTF-8, percorso risorse configurabile e distribuzione.
5. Misurare prima di ottimizzare loop inattivo, caricamenti e ricreazioni;
   aggiungere smoke runtime con validation/synchronization validation e prove
   resize/minimize/restore, distinguendole dai test headless.

Non serve una riscrittura generale per correggere i primi problemi. Occorre
prima stabilizzare i contratti di stato, ownership ed errore che oggi sono
impliciti nel codice estratto dal motore.
