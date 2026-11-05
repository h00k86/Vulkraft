# Comportamento UI per l’integrazione host

Versioni: UIFramework 0.3.1 e GraphicsApp 0.5.1. Il codice dell’host non viene
modificato in questo repository. Il commit preciso da importare accompagna
questa consegna; aggiornare la copia locale in modo esplicito.

## Esc e modali

Una pressione di Esc chiude la modale attiva, anche se un campo testo, password
o numerico ha il focus. `ui_modal_begin` restituisce false e imposta il flag
`is_open` a false; non comporre il contenuto in quel ramo. L’evento viene
consumato e `UIInputDiagnostics.escape_consumed` rimane true fino al frame
successivo: il dispatcher dell’host non deve inoltrare lo stesso Esc ai comandi
sottostanti. Il batch originale del chiamante rimane invariato perché la UI
lavora sulla propria copia degli eventi.

Menu e dropdown aperti sono overlay sopra il dialogo: Esc chiude prima quello
attivo, mantenendo aperta la modale. Con modali annidate, una pressione chiude
soltanto quella più interna. Fuori dalle modali, Esc conserva il comportamento
dell’editor di testo (ripristino dello snapshot) e del campo numerico.

Il toolkit non possiede il modello del dialogo: chiusura e consumo dell’evento
non effettuano rollback dei buffer già modificati. Per Rinomina usare una copia
di lavoro, applicandola soltanto alla conferma; scartarla all’annullamento.

## Focus e metodo di input

La selezione del controllo e la visibilità dell’anello sono distinte. Tab e le
azioni di navigazione da tastiera mostrano l’anello accent. Movimento, click,
rilascio e rotella del mouse lo nascondono mantenendo il destinatario del
successivo Tab. Autofocus programmato e ritorno da una modale non forzano
la modalità tastiera. La perdita di focus della finestra continua a cancellare
focus e gesti pendenti.

L’host può continuare a dare subito il focus al campo Rinomina con
`ui_text_input_focus`: il cursore è disponibile senza imporre un anello da
navigazione. La scelta del campo iniziale resta dell’applicazione.

## Cattura e fine dell'editing

Il focus di navigazione e la cattura della tastiera sono distinti.
`ui_has_keyboard_capture()` resta false dopo un clic su un normale bottone
rilasciato. Campi testo/numerici in editing e overlay aperti catturano sempre;
la navigazione esplicita da tastiera continua a catturare.

Fuori dalle modali, Invio conferma ed Esc annulla l'editing: entrambi rilasciano
la cattura mantenendo la posizione per Tab. Le successive scorciatoie
(es. G/R dell'editor) non riattivano il campo. Un click, Tab o Invio possono
riaprirlo; per i campi testo è disponibile anche `ui_text_input_focus`.
Un numero non valido resta invece in editing e continua a catturare.
La presenza di una modale o di un altro overlay mantiene la sua cattura anche
quando termina l'editing del campo interno.

Il chiamante deve distinguere la cattura corrente dall'evento che l'ha appena
terminata: per Esc resta valido `UIInputDiagnostics.escape_consumed`. Il batch
originale rimane invariato. Questa correzione non modifica il prepass delle
regioni di input dell'host, necessario per evitare click attraverso gli overlay.

## Numeri nei campi e negli slider

I float a riposo e all'apertura dell'editor usano una forma compatta: almeno
sei cifre significative disponibili, aumentate solo se necessario a ricostruire
lo stesso float. `%g` elimina gli zeri finali: `2.2f` appare `2.2`, mantenendo
il valore nel modello. Il formato può usare la notazione scientifica, ammessa
anche durante la digitazione. Il testo è ritagliato all'interno del campo;
mentre si scrive scorre verso la fine, dove vengono aggiunti i caratteri.

## Etichette

Per default il toolkit conserva il testo fornito: `3D`, `FX` e `Nuovo layer`
restano esattamente così. Questo non cambia gli ID dei widget.

```c
ui_context_set_current(context);
ui_set_sentence_case(false); /* default; esplicito per gli adattatori host */
```

`ui_set_sentence_case(true)` abilita una normalizzazione ASCII delle etichette
interamente maiuscole: `NUOVO LAYER` diventa `Nuovo layer`. Le etichette già
miste e i contenuti tra parentesi restano invariati; non è Unicode case folding.
La preferenza appartiene al contesto corrente e `ui_context_reset` la disattiva.
`ui_humanize_label` rispetta questa preferenza. I campi di testo non subiscono
trasformazioni. I buffer di output restano terminati e il troncamento conserva
i confini UTF-8.

Per conservare le sigle con sentence case attivo, l'host può fornire un elenco:

```c
static const char *const acronyms[] = { "FX", "VFX", "GLB", "FPS", "HUD", "UI", "NPC" };
ui_context_set_current(context);
if (!ui_set_sentence_case_exceptions(acronyms,
                                    sizeof(acronyms) / sizeof(acronyms[0]))) {
    /* Configurazione non valida: gestire l'errore nell'host. */
}
ui_set_sentence_case(true); /* MOSTRA FX -> Mostra FX */
```

L'elenco iniziale è vuoto e non abilita la normalizzazione. Array e stringhe
sono presi in prestito: devono restare vivi e invariati fino a sostituzione,
reset o distruzione del contesto. Usare storage statico per le sigle del motore.
Il toolkit non include un vocabolario specifico dell'applicazione.

Le voci sono parole ASCII non vuote composte da lettere, cifre o underscore;
il confronto è esatto e sensibile alle maiuscole. Punteggiatura e spazi separano
le parole: `FX/VFX` conserva entrambe, mentre `AFX`, `FX2`, `FX_TEST` e parole
UTF-8 che inglobano `FX` non corrispondono alla voce `FX`. Il riconoscimento
considera la parola completa anche se il buffer di output la tronca.
`ui_set_sentence_case_exceptions(NULL, 0)` svuota l'elenco. Un array NULL con
conteggio non zero, una voce NULL/vuota o contenente separatori restituiscono
false e lasciano intatta la configurazione precedente. Il reset svuota l'elenco
e disabilita sentence case. Nessuna allocazione è necessaria.

Nell'adattatore del motore riportare le sigle effettivamente protette dalla sua
funzione `keep_upper_word`: `PREFAB` compare nell'array storico ma viene escluso
esplicitamente dal codice, quindi non va aggiunto automaticamente alla nuova
lista. La nuova API non replica altre euristiche del motore sulle singole
lettere o sulle parole contenenti numeri: includere le parole desiderate o
conservarle direttamente nell'etichetta.

## Prova da ripetere nell’host

- Click e rilascio su bottone: nessuna cattura residua; G/R disponibili.
- Campo numerico fuori dalle modali: Invio/Esc rilasciano la cattura; un G/R
  successivo non riprende l'editing. Verificare anche ritorno con Tab e Invio.
- Slider con `2.2f`, valori molto piccoli/grandi e campo stretto: testo confinato
  al campo, conferma senza modifiche non arrotonda il valore nel modello.
- Campo Rinomina con testo modificato: Esc chiude con una pressione, non
  conferma il nome e non attiva comandi dell’interfaccia sottostante.
- Dropdown in una modale e modali annidate: si chiude un solo livello per Esc.
- Apertura/cambio focus con mouse, Tab, ritorno dalla modale e cambio contesto:
  destinazione corretta e anello coerente col metodo di input.
- Testi disegnati: `3D`, `FX`, `Nuovo layer` e input UTF-8 invariati.

Le regressioni nel toolkit sono in `tests/ui_navigation_test.c` e
`tests/ui_overlay_modules_test.c`. Non sostituiscono la prova del pannello host.
Troncamento geometrico e fine corsa dello scroll non sono modificati da questa
consegna: metriche, padding e adattatore dell’host richiedono il confronto dedicato.
