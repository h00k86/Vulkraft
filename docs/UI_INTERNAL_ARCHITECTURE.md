# Proprietà dello stato e contratti interni

Solo `ui.c` include `ui_context_internal.h` e conosce il layout fisico del
contesto. `UIContext` resta opaco negli header pubblici e ora anche nei contratti
tra moduli. La memoria è incorporata nello stesso oggetto del contesto:
le nuove strutture non aggiungono allocazioni o copie dei registri.

## Strutture per responsabilità

Gli header `ui_state_*_internal.h` dichiarano una struttura e il relativo
accessor privato `ui_state_*(context)`. Le aree comprendono frame/input,
navigazione, pulsanti, campi numerici, slider, campi testo, menu, dropdown,
modali, pannelli, tooltip, renderer, clipboard, ID, stile/notifiche, picker e
stato diagnostico. Il tipo comune delle voci dei registri è dichiarato in
`ui_state_types_internal.h`, che non definisce UIContext.

Ogni modulo include soltanto le strutture necessarie. Esempi:

| Modulo | Stato direttamente accessibile |
| --- | --- |
| Menu | Frame, gerarchia menu |
| Dropdown | Frame, navigazione, dropdown |
| Pulsanti | Frame, navigazione, pulsanti, tooltip |
| Controlli numerici | Frame, navigazione, campi numerici, slider |
| Campo testo grafico | Frame, navigazione, campi testo, clipboard |
| Picker opzionale | Picker, proprietà dell'anteprima del renderer |

Navigazione e cattura dell'input coordinano più aree per gestire focus,
annullamento e priorità. Non possiedono copie dei dati dei widget.
`tools/check_boundaries.py` definisce le dipendenze ammesse, impedisce ai moduli
di importare il layout fisico e verifica la separazione dai backend.

## Durata delle viste

Gli accessor richiedono un contesto valido e non NULL. Le viste sono prese in
prestito: restano allo stesso indirizzo durante la vita del contesto, anche
attraverso un reset; il reset azzera i dati e invalida il significato delle
precedenti voci di widget. Una vista non può essere usata dopo la distruzione.
I puntatori alle singole voci possono inoltre essere riutilizzati dai registri
limitati. Gli accessor non trasferiscono proprietà né rendono la UI thread-safe.
Le viste usate ripetutamente dai controlli sono conservate in variabili locali.

Il contesto continua a possedere snapshot e cronologie dei campi testo, memoria
del picker, copie dinamiche delle opzioni dei dropdown e stato delle anteprime;
reset/destroy applicano gli stessi contratti
di rilascio. Renderer e clipboard conservano la proprietà definita dalle API
pubbliche. Questi header privati sono nella distribuzione sorgente per costruire
la libreria, ma non vengono installati come API dell'SDK.

I dropdown conservano nel contesto voci stabili e metadati; il buffer delle
opzioni viene allocato solo quando si prepara un popup aperto. Cresce per le
righe visibili, da 8 fino a 128 righe di 128 byte; chiusura e riuso della voce
di cache conservano la capacità. Reset/destroy rilasciano ogni buffer prima
di azzerare i domini. Se realloc fallisce, si conserva il vecchio buffer, si
chiude il popup e si dimentica il precedente proprietario della cattura; il
click di apertura resta consumato. Non viene introdotto uno storage globale
condiviso né una nuova dipendenza dei moduli dalla struttura fisica del contesto.

Anche le copie degli owner degli eventi e le voci dei menu sono allocate su
richiesta. La navigazione riserva uno slot per evento del batch effettivo,
con ID completi e ordine invariato; il core esegue la riserva prima di aprire
il frame. Il contratto privato `ui_nav_begin()` restituisce false se la riserva
fallisce, senza modificare il routing esistente.

I menu conservano nel contesto i metadati stabili dei quattro livelli e delle
sedici dichiarazioni di sottomenu. Ogni payload di voci cresce da 4 a 32 elementi
al primo uso; chiusura e aggiornamento conservano capacità e proprietà.
Un OOM della radice o della dichiarazione chiude la gerarchia; un OOM del
figlio chiude quel ramo conservando il genitore. La diagnostica `overflowed`
segnala anche la riserva fallita. Gli eventuali click restano consumati.
Reset/destroy liberano tutti i payload dei livelli e delle dichiarazioni,
anche se la gerarchia è già chiusa. Le callback del renderer restano sincrone.

## Controlli compilati separatamente

`ui_buttons.c`, `ui_numeric_controls.c` e `ui_text_field.c` sono unità C autonome.
Radio/tooltip e tab/splitter sono incorporati nelle rispettive unità: non si
includono più header contenenti queste implementazioni. Gli header pubblici
non cambiano; i campi password passano ancora dal filtro copy/cut del widget.
Le intestazioni comprimibili appartengono al modulo dei pannelli.

## Test dei contratti

Undici suite ora collegano la libreria invece di includere `ui.c`, anche
attraverso fixture: navigazione, interazione, tab/splitter, basic widget,
editing, editing APP, renderer, frame input, draw list, config editor UI e
color picker. Le verifiche che richiedono conteggi o snapshot usano le strutture
private dedicate. Il test draw-list conserva l'inclusione della sola relativa
implementazione per l'iniezione degli errori di allocazione.

`ui_navigation_batch_test` confronta batch misti, batch massimi, ID lunghi e
retarget da mouse; verifica anche rifiuto transazionale del frame su OOM,
riuso della capacità e cleanup. `ui_menu_storage_test` verifica copie dei
payload, radice/figli/dichiarazioni, crescita e fallimenti, limiti e isolamento.
Entrambi collegano il core; i test di storage Linux usano wrapper realloc/free.

`ui_state_contract_test` verifica isolamento dei contesti, ID uguali con stato
indipendente, stabilità delle viste, pulizia degli snapshot e mantenimento
della clipboard al reset, senza includere il layout del contesto.
Restano quattro fixture legacy che includono direttamente il core: registri
/input, sicurezza del buffer, piattaforma headless e probe GLFW. Non sono state
eliminate le loro regressioni. Il controllo architetturale impedisce alle suite
migrate di reintrodurre l'inclusione diretta o transitiva di `ui.c`.

## Diagnostica senza dipendenze dal layout

Il dominio `UIErrorsState` appartiene al contesto ed è accessibile fisicamente
solo da `ui.c`. I moduli segnalano errori tramite `ui_error_internal.h`, senza
importare il relativo store. Il valore pubblico contiene codice, operazione e
ID copiato; non introduce heap. Il rifiuto del frame resta atomico e comunica
la causa tramite il risultato, senza aggiornare questo dominio.
Contratti e copertura in [UI_ERRORS.md](UI_ERRORS.md).
