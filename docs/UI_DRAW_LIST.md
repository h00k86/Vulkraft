# Draw list UI

`UIDrawList` è un backend intermedio: registra le operazioni dei widget e le
riproduce in ordine sul `UIRenderer` scelto. La produzione conserva il percorso
immediato predefinito; la registrazione è una scelta esplicita del chiamante.

## Ciclo di vita

```c
UIDrawList *list = ui_draw_list_create(ui_app_renderer(), app);
if (!list || !ui_context_set_renderer(app->ui, ui_draw_list_renderer(), list)) {
    /* Gestire l'errore e liberare list se creata. */
}
```

Creare la lista e collegarla fuori da un frame UI aperto. Il target e il suo
`user` restano presi in prestito. La tabella delle callback viene copiata.
Una lista non può essere usata come target di se stessa o coinvolta in un ciclo
di backend. Non chiamare le sue API in modo rientrante dalle callback.

Per ogni frame:

1. Attendere le risorse grafiche e iniziare il frame del target (`text_begin`
   nell'adattatore esistente).
2. Chiamare `ui_draw_list_begin(list)`, che scarta la precedente registrazione,
   azzera gli errori e conserva la capacità allocata dei comandi.
3. Comporre i widget, inclusi i popup differiti, e completare il frame UI.
4. Chiamare `ui_draw_list_replay(list)`. Solo ora i comandi arrivano al target.
5. Se riesce, completare preparazione e submit del renderer; altrimenti scartare
   il frame grafico. Non presentare geometria parziale dopo un errore.

`replay` è consumabile una sola volta per `begin`: scarta testi e comandi logici,
conservando i buffer allocati. Non è un archivio serializzato o una lista da
riprodurre indefinitamente. `ui_draw_list_count()` conta i comandi ancora in coda;
`ui_draw_list_error()` espone lo stato di errore fino al prossimo `begin`.

`ui_draw_list_discard(list)` abbandona la registrazione senza aprirne una nuova:
scarta testi e comandi logici, esegue i rilasci delle preview rinviati e conserva
capacità allocata ed errore corrente. È ripetibile; rifiuta NULL e chiamate durante
il replay. Dopo discard occorre un nuovo begin prima di registrare o riprodurre.
Non modifica i clip del target e non annulla disegni già riprodotti. Se il replay
è stato tentato, prima completare o scartare il frame del target. Per SDL:

```c
/* Dopo aver chiuso il frame UI, in caso di errore del replay o del target: */
ui_sdl2_discard(backend);
ui_draw_list_discard(list);
/* Ora si può ridimensionare, oppure iniziare il prossimo frame. */
```

Prima di distruggere la lista, scollegarla dal contesto o distruggere il contesto.
Completare il submit dell'ultimo frame prima di iniziare/scartare un'altra lista
o liberare risorse. Il target grafico deve sopravvivere alla lista.

## Proprietà dei dati

Coordinate, colori, array decorativi e stringhe sono copiati. Il widget può
quindi usare etichette locali che cambiano o scompaiono prima del replay.
Le stringhe sono copiate in un'arena contigua che cresce geometricamente fino
a 16 MiB, terminatori inclusi. I comandi conservano offset, validi anche se
l'arena cambia indirizzo. Replay, begin e discard azzerano l'utilizzo logico;
la capacità raggiunta rimane disponibile fino alla distruzione della lista.
Un frame che rientra nella capacità già raggiunta per testi e comandi non
richiede allocazioni della draw list. Le callback del target restano sincrone:
non devono trattenere i puntatori alle stringhe oltre il replay.

`Font*`, atlas e handle delle immagini sono presi in prestito: devono restare
validi e coerenti fino al replay e, per le risorse GPU, fino al completamento
richiesto dal renderer. La misurazione del testo e le query sulle immagini
restano sincrone, perché il layout ne ha bisogno subito.

Lo stack di clipping è registrato in ordine. Il replay richiede push/pop
bilanciati e la stessa profondità iniziale sul target. Il backend esegue le
intersezioni; la lista non sostituisce la sua semantica di clipping.
Non mescolare disegno diretto e comandi registrati aspettandosi un ordine
intercalato: il disegno diretto precede il replay della lista.

## Anteprime e fallimenti

Il caricamento dell'anteprima è sincrono. Dopo aver registrato un'immagine, la
lista rifiuta ulteriori caricamenti della preview transiente: sostituire lo
slot renderebbe invalidi i comandi già registrati. Questo controllo è prudente
e vale anche se l'immagine precedente non era una preview.

La cancellazione dell'anteprima viene rinviata quando esistono immagini in coda
o appena riprodotte. Il rilascio avviene al successivo `begin`, `discard` o alla distruzione,
dopo che il chiamante ha inviato o scartato la geometria precedente. Il replay
prepara geometria, non costituisce un submit GPU.

La lista limita ogni registrazione a 65.536 comandi, 16 MiB di testi e 31 livelli
di clipping aggiuntivi. Allocazioni fallite, superamento dei limiti e uso non
valido producono un errore persistente. Un errore di registrazione blocca il
replay prima del primo comando. Un rifiuto del clipping da parte del target
può invece avvenire dopo alcuni comandi: viene tentato il ripristino dello stack
e il replay restituisce false. Le callback di disegno void conservano il sistema
di errori del target; nell'adattatore APP occorre controllare anche `app->error`.

## Verifica

`tests/ui_draw_list_test.c` usa widget reali e un target simulato: copia UTF-8 e
array, differimento del disegno, ordine del clipping, errori di allocazione,
stack sbilanciati, rifiuti del target e durata delle anteprime.

La scena Vulkan di regressione ha una variante che registra i widget e i popup
prima del replay, senza cambiare il loop applicativo predefinito:

```sh
make build/draw-list-runtime.out build/glfw-key-event-probe.out
python3 tools/runtime_validation_passive.py --unicode --draw-list \
    --output evidence/ui-draw-list/runtime
```

La variante riproduce la lista dentro `app_draw`, dopo aver registrato tutti i
popup; `ui_end_frame` resta al loop. L'esempio completo di collegamento e
scollegamento è in `tests/ui_draw_list_runtime.c`.
Le evidenze sono in [evidence/ui-draw-list](../evidence/ui-draw-list/).
Restano i vincoli del backend Vulkan condiviso: questa funzionalità non aggiunge
thread di rendering, più finestre simultanee o un secondo backend grafico.

La callback `font_ascent`, come `measure_text_width`, interroga subito il backend:
serve al layout e non aggiunge comandi alla lista. Il font è opaco alla UI;
vedi [contratto dei font](UI_FONT_BACKEND.md).

Le callback di decorazione del target sono opzionali. La lista conserva i
comandi di decorazione e applica gli stessi fallback del percorso immediato
al replay; una decorazione può produrre più primitive sul target.
Vedi [decorazioni opzionali](UI_OPTIONAL_SKIN.md).

Il primo errore viene conservato fino al begin successivo; dopo un errore non
si avviano nuovi caricamenti di anteprime, ma il cleanup resta disponibile.
Vedi [gestione degli errori](UI_DRAW_LIST_ERRORS.md).
