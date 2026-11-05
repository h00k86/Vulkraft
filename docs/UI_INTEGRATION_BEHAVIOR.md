# Comportamento UI per l’integrazione host

Versioni: UIFramework 0.2.0 e GraphicsApp 0.4.0. Il codice dell’host non viene
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

## Prova da ripetere nell’host

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
