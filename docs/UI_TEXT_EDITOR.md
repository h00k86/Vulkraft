# Editor di testo multilinea

API pubblica: `ui_text_editor` in `include/ui/ui.h`. Incluso in
`UIFramework::core`, senza dipendenze GLFW, Vulkan o filesystem.

```c
/* Persistono nell'host, non sullo stack temporaneo di ogni frame. */
static char source[UI_TEXT_EDITOR_MAX_CAPACITY] = "void main() {\n\t// prova è\n}\n";
static int error_line;   /* 0 oppure indice a partire da 1 */
static int jump_line;    /* comando di un solo frame */

UITextEditorOptions options = {
    .scale = 14,          /* adatto al backend SDL; usare la scala del proprio font */
    .line_numbers = true,
    .highlight_line = error_line,
    .reveal_line = jump_line
};
UITextEditorResult edit = ui_text_editor("shader/document-42", font,
    (UIRect){20, 60, 600, 360}, source, sizeof(source), &options);
jump_line = 0;
if (edit.changed) { /* aggiornare il documento dell'host */ }
if (edit.save_requested) { /* chiedere al servizio documenti dell'host di salvare */ }
```

La chiamata avviene dentro un frame UI attivo e un contesto selezionato, come gli
altri widget. Il renderer deve avere il clipping. Non chiamarla due volte con lo
stesso ID nello stesso frame e non riusare lo stesso ID per widget di tipo diverso.
Il buffer mutabile deve restare valido per tutta la chiamata. Non è trattenuto
fra frame; dimensione e contenuto possono essere aggiornati dall'host fra frame.
L'ID identifica un documento: usare ID diversi per mantenere le storie separate.

## Testo, coordinate e risultati

- Capacità fra 1 e 32768 byte **incluso NUL**, testo UTF-8 valido e terminato
  entro quella capacità. LF separa le righe; il caricatore dell'host normalizza
  CRLF/CR a LF e rifiuta file binari/NUL incorporati. Il widget rifiuta CR nel
  buffer iniziale; la clipboard normalizza CRLF e CR automaticamente.
- Un'inserzione a capacità esaurita viene ignorata senza scrivere metà di uno
  scalare UTF-8. L'incolla inserisce il prefisso valido che entra nel buffer.
- `caret` e `selection_anchor` sono offset in byte UTF-8. `line`, `column` e
  `line_count` includono righe vuote e partono da 1. La colonna conta scalari
  Unicode: un tab conta una colonna logica, ma occupa un tab stop di quattro spazi
  sullo schermo. Non è un conteggio di grafemi o di colonne visuali.
- `changed` confronta il contenuto all'ingresso e all'uscita della chiamata:
  movimento, selezione e scroll non lo impostano. Non è un flag di documento
  sporco persistente. L'host confronta con l'ultima versione salvata, anche
  dopo undo/redo, e gestisce gli eventuali cambiamenti esterni ai file.
- `save_requested` è un impulso Ctrl+S; non scrive né compila nulla. Vale anche
  in sola lettura: l'host decide se offrire copia/esportazione o ignorarlo.
- `reveal_line > 0` sposta cursore e scroll alla riga richiesta, senza rubare
  focus e senza cambiare il testo; oltre l'ultima riga raggiunge l'ultima.
  Azzerarlo nel frame successivo. `highlight_line` è invece persistente.
- Opzioni NULL/azzerate: modificabile, senza numeri, scala `theme.text_scale`.
  `scale` deve essere finita e non negativa. Il rettangolo assoluto deve avere
  coordinate finite e almeno 32x32 unità del canvas.

## Interazione

- Click posiziona il cursore; trascinamento seleziona anche oltre le righe e fa
  scorrere il viewport ai bordi. Larghezza del font e tab stop guidano l'hit test.
- Frecce e Shift+frecce; Su/Giù mantengono la colonna logica preferita passando
  per righe corte. Home/End: riga; Ctrl+Home/End: documento. PageUp/PageDown:
  circa una pagina. Shift estende anche queste selezioni.
- Invio inserisce LF; Tab inserisce un tab letterale. Shift+Tab passa al widget
  precedente. Esc termina l'editing conservando il testo; dopo Esc Tab torna
  a navigare. Non esiste ancora indentazione/dedentazione di blocchi.
- Ctrl+A/C/X/V, Ctrl+Z/Y e Ctrl+Shift+Z usano i servizi esistenti del contesto.
  Ctrl+Alt non attiva queste scorciatoie, per non interferire con AltGr.
- Rotella verticale, scrollbar verticali/orizzontali trascinabili e rivelazione
  automatica del cursore dopo movimento/modifica. Nessun a capo automatico.
- `read_only` permette navigazione, selezione e copia; blocca modifica,
  incolla, taglia e undo/redo. Le azioni di salvataggio restano dell'host.

Focus e cattura usano `ui_text_input_focus(id)`, `ui_text_input_is_focused(id)`,
`ui_any_text_input_focused()` e `ui_has_keyboard_capture()`. Rilascio cattura,
perdita focus finestra, modali e reset seguono il contratto comune. Il motore
registra le regioni UI prima del proprio update e sospende i comandi della scena
quando la UI cattura l'input; consumare gli eventi del widget non basta a bloccare
il polling globale dei tasti dell'host.

## Risorse e limiti

Lo stato è per contesto/ID, nella cache dei campi testo (32 campi condivisi).
Le revisioni sono snapshot interi: massimo 17 revisioni (16 passi) e 64 KiB totali
per campo. Vicino al limite di 32 KiB restano meno passi, fino a un solo undo.
L'allocazione fallita può svuotare la storia, come nei campi monoriga; non è
un editor per documenti grandi. Reset/destroy liberano revisioni e stato.
Un cambiamento esterno del contenuto invalida la storia, senza simulare un edit.

Il modello viene scandito/validato a ogni chiamata; solo le righe nel viewport
vengono disegnate. Gli snapshot allocano durante le modifiche; non si promettono
editing senza allocazioni o prestazioni da editor per file di grandi dimensioni.

Non include syntax highlighting, ricerca/sostituzione, completamento, shaping,
grafemi, bidi, IME preedit, caricamento/salvataggio, compilatore o scripting.
Gli accenti da eventi UTF-8 sono coperti dai test; la digitazione con tastiera
fisica e sistema operativo resta una prova separata. Il font dell'host deve avere
i glifi desiderati. La demo è nella scheda **Editor testo** del dialogo Componenti
(`make ui-reference-sdl2` o `make ui-reference-vulkan`).

Per l'integrazione nel motore: [PROMPT_ENGINE_TEXT_EDITOR.md](PROMPT_ENGINE_TEXT_EDITOR.md).

Da UIFramework 0.8.0 le modali riprendono l'editing sospeso, conservando cursore,
selezione e viewport. Se il dialogo è aperto da una barra o da un menu, l'host
può scegliere il documento di ritorno: [UI_PICKER_FOCUS.md](UI_PICKER_FOCUS.md).
