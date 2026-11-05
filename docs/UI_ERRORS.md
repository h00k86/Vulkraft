# Diagnostica del core UI

`ui/error.h` definisce `UIErrorCode`, `UIErrorOperation` e `UIErrorInfo`.
La diagnostica non alloca memoria e non richiama il backend.

## Ammissione del frame

`ui_begin_frame_input_status(input)` restituisce:

| Codice | Causa |
| --- | --- |
| `UI_ERROR_NONE` | Frame aperto correttamente |
| `UI_ERROR_STATE` | Nessun contesto corrente oppure frame già aperto |
| `UI_ERROR_ARGUMENT` | Input nullo, dimensioni/coordinate/tempo non validi oppure batch senza puntatore |
| `UI_ERROR_LIMIT` | Batch maggiore di `INPUT_KEYBOARD_CAPACITY` |
| `UI_ERROR_MEMORY` | Impossibile riservare gli owner degli eventi |

Le verifiche avvengono nell'ordine stato, puntatore input, limite batch,
restanti argomenti, memoria. Il fallimento lascia invariati contesto e input,
compresa la diagnostica del contesto. Il chiamante deve leggere il risultato;
`ui_begin_frame_input()` conserva il contratto booleano precedente.

```c
UIErrorCode status = ui_begin_frame_input_status(&input);
if (status != UI_ERROR_NONE) {
    fprintf(stderr, "UI begin: %s\n", ui_error_code_name(status));
    /* Nessun end_frame per questo tentativo fallito. */
}
```

## Errori del contesto e dei widget

`ui_context_error(context)` restituisce per valore il primo errore segnalato,
con operazione e copia dell'ID (massimo 255 byte più terminatore).
Il valore iniziale è `UI_ERROR_NONE`; le chiamate riuscite e l'inizio di un
nuovo frame non lo cancellano. `ui_context_clear_error(context)` e un reset
riuscito lo azzerano. Ogni contesto conserva il proprio errore.

Copertura attuale:

- cambio/reset/distruzione del contesto e binding renderer/clipboard rifiutati
  per frame attivo; tabella renderer incompleta;
- ID non valido e saturazione dei registri pulsanti, testo e dropdown;
- buffer/capacità testo e argomenti delle opzioni dropdown non validi;
- memoria insufficiente per le copie delle opzioni dropdown;
- argomenti, limite voci/dichiarazioni/profondità e allocazioni dei menu;
- ID non valido e saturazione dello stack degli ID.

Un `false` di un widget può indicare semplicemente assenza di interazione;
non costituisce da solo un errore. La diagnostica è persistente: dopo averla
letta e gestita, cancellarla esplicitamente prima di cercare un nuovo errore.
Gli esiti preesistenti restano compatibili: per esempio, i menu oltre il
limite vengono troncati e `overflowed` resta disponibile; il codice tipizzato
permette ora di distinguere quel caso da un'allocazione fallita.

Non è una diagnostica universale per tutte le funzioni: cronologia/editing,
clipboard dell'host, picker e backend conservano i propri contratti.
La draw list mantiene `UIDrawListError`, con le sue regole di replay/discard.
Gli argomenti grafici di ogni widget non sono tutti validati da questa API.
`ui_context_create()` continua a restituire NULL su OOM: non esiste ancora un
contesto in cui registrare l'errore.

Le operazioni di contesto rifiutate possono aggiornare solo la diagnostica.
Gli errori nell'ammissione del frame fanno eccezione: usano esclusivamente
il valore restituito per conservare l'atomicità del rifiuto.
I vincoli di thread principale, contesto corrente e assenza di rientranza
rimangono validi. Getter con NULL restituisce ARGUMENT/CONTEXT; clear con NULL
non esegue operazioni.

## Verifica

`ui_error_test` controlla codici, fallimenti di allocazione, rifiuto atomico,
persistenza del primo errore, proprietà dell'ID, isolamento e reset.
`ui_session_test` esegue tre sequenze deterministiche di 9.216 frame ciascuna:
due contesti alternati, ID che superano nel tempo le capacità dei registri,
editing UTF-8, batch, perdita del focus, dropdown, menu e rendering immediato
o differito. Ogni ciclo resetta entrambi i contesti e verifica che i byte
ancora allocati tornino al valore iniziale; la distruzione deve portarli a zero.

Il contatore usa i wrapper GNU ld già impiegati dal benchmark. Osserva le
chiamate collegate staticamente, non la memoria residente né le allocazioni
interne di libc/driver. Non sostituisce LeakSanitizer o una sessione nativa.
