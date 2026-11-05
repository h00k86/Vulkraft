# Tab e splitter

Controlli del core UI indipendenti da APP, GLFW e SDL. Devono essere chiamati
in un frame UI con ID stabili e distinti; usare `ui_scoped_id` per istanze ripetute.
Il chiamante conserva selezione e proporzione e disegna solo la pagina attiva.

```c
const UITab pages[] = {{"Generale", false}, {"Dettagli", false}};
ui_tabs("settings/tabs", font, (UIRect){20, 20, 600, 32}, pages, 2, &page);
UIRect first, second;
ui_splitter("settings/split", (UIRect){20, 60, 600, 400},
            UI_SPLIT_COLUMNS, 8, 120, 160, &ratio, &first, &second);
/* Disegnare i pannelli usando first e second; installare i rispettivi clip. */
```

`page` e `ratio` sono variabili persistenti dell'applicazione, inizializzate per
esempio a 0 e 0.5. Le funzioni restituiscono true quando cambia il valore,
compresa la normalizzazione di valori fuori intervallo.

## Tab

Larghezza uniforme, fino a 64 schede; le etichette sono ritagliate nella propria
area. Il mouse seleziona alla pressione. Tab raggiunge l'intera barra con un
solo passo; Sinistra/Destra scorrono ciclicamente tra le schede abilitate e
Home/End selezionano prima/ultima. Una selezione invalida diventa la prima
abilitata; se tutte sono disabilitate, diventa -1. I contenuti delle schede non
sono gestiti dalla barra: il chiamante sceglie cosa disegnare.

## Splitter

`UI_SPLIT_COLUMNS` produce due colonne; `UI_SPLIT_ROWS` due righe. La proporzione
si riferisce allo spazio disponibile dopo aver sottratto lo spessore della
maniglia. Il trascinamento mantiene il punto di presa senza salto iniziale.
Le frecce lungo l'asse spostano dell'1%; Home/End raggiungono i limiti.

I minimi sono espressi nelle coordinate UI. Se non entrano nell'area, vengono
ridotti proporzionalmente; i rettangoli restano non negativi. Lo spessore della
maniglia viene limitato alla dimensione dell'area. Un rapporto non finito viene
inizializzato al centro dell'intervallo consentito. Gli argomenti invalidi
producono false e rettangoli di uscita vuoti.

Entrambi rispettano i modali e i clip aperti con `ui_clip_push` o gli scope
panel/scroll. Lo splitter conserva la cattura durante il trascinamento anche
fuori dalla maniglia; perdita del focus annulla il gesto. Il rapporto resta
quello raggiunto, senza rollback. Non comprende docking, persistenza automatica,
riordino o chiusura delle tab, né cursori nativi di ridimensionamento.

La scena di riferimento condivisa SDL/Vulkan mostra entrambi i controlli in
fondo al contenuto scorrevole: le tab selezionano colonne o righe.

## Verifiche

`build/ui_tabs_splitter_test` esercita il core con renderer deterministico;
`build/ui_tabs_splitter_test_sanitize` aggiunge ASan/UBSan (LSan disabilitato).
Il test è incluso in `make test`, `make test-medium-sanitize` e CTest.
