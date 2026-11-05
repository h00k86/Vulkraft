# Supporto Unicode/UTF-8 — 27 ottobre 2026

Editor e renderer accettano UTF-8; i caratteri confermati dalla callback GLFW
vengono codificati senza passare attraverso una conversione ASCII.

## Contratto

- I buffer pubblici restano `char *` terminati da NUL. Capacità e posizioni
  interne sono espresse in byte; il cursore si muove su confini di punti di codice.
- Inserimento atomico: se manca spazio per il carattere intero e il terminatore,
  il carattere viene ignorato. Backspace/Delete rimuovono un punto di codice.
- I campi testuali rifiutano buffer non terminati o UTF-8 malformato senza modificarli.
  I controlli C0/C1 e i separatori di linea non sono inseribili nel campo monoriga.
- Il renderer sostituisce byte malformati con U+FFFD, avanzando di un byte per
  errore. Misura e disegno condividono decodifica e scelta del glifo.
- Per glifi mancanti: U+FFFD, poi `?`; se il font non contiene nessuno dei due,
  il carattere non produce geometria o avanzamento. Il testo originale resta intatto.
- Scorrimento del testo e abbreviazione con `...` rispettano i confini UTF-8.
  Il prefisso usato per misurare il cursore non è più limitato a 255 byte.
- Escape conserva anche snapshot superiori a 255 byte. Le copie allocate vengono
  liberate a fine modifica, perdita di focus, riuso dello stato o shutdown.
  `ui_text_input_reset()` cancella gli stati testuali del contesto corrente.
  Reset e distruzione di [UIContext](UI_CONTEXT.md) liberano anche questi snapshot.
- I campi numerici mantengono la grammatica numerica ASCII esistente.

Non si tratta di un motore completo di composizione tipografica: non sono inclusi
segmentazione in grafemi, normalizzazione, shaping, BiDi o preedit IME.
La clipboard è ora disponibile tramite callback dell’host: vedere
[UI_TEXT_EDITING.md](UI_TEXT_EDITING.md).
Un accento combinante o una sequenza emoji può quindi richiedere più pressioni di
Backspace; le sequenze confermate dal sistema arrivano come singoli punti di codice.
Non è stata verificata una sessione IME reale. I percorsi nativi Windows non sono
stati convertiti alle API wide in questo intervento.

## Font

Il nuovo atlante predefinito `assets/fonts/font_utf8.{json,png}` contiene 321 glifi:
ASCII, Latin-1 stampabile, Latin Extended-A, euro e U+FFFD. È 1024×1024 MSDF,
64 pixel/em e range 8; la dimensione per carattere e le metriche del precedente
Open Sans Italic sono conservate. Gli asset `font2.*` restano disponibili.

Il sorgente `OpenSans-Italic.ttf` e la licenza `OpenSans-OFL.txt` sono inclusi.
Rigenerazione dalla radice, con msdf-atlas-gen 1.4.0 / msdfgen 1.13.0:

```sh
sh tools/generate_font_atlas.sh /percorso/msdf-atlas-gen
```

`Font.glyphs` ora è una tabella ordinata con `glyph_count`: usare `font_get`,
non indicizzare con il valore Unicode. Il limite `MAX_GLYPHS=512` conta i glifi,
non limita i punti di codice a 511. Il caricamento accetta anche punti di codice
supplementari, rifiuta duplicati/eccessi di capacità e conserva il vecchio font
in caso di fallimento. Per ampliare la copertura occorre rigenerare l'atlante;
non è incluso un caricamento dinamico dei glifi.

## Verifiche

- Build debug e release; confini architetturali e `git diff --check`.
- 16 eseguibili headless; 14 con ASan/UBSan, LeakSanitizer disabilitato.
- Round-trip di tutti i valori scalari non NUL, rifiuto di surrogate,
  sequenze overlong/troncate e valori oltre U+10FFFF.
- Editing misto con caratteri da 2/3/4 byte, cursore, cancellazione, buffer pieni,
  riallineamento dopo sostituzione esterna, Escape su 600 byte, ellissi in-place.
- Renderer reale con font caricato: una quad per punto di codice, larghezza
  coerente, fallback per caratteri assenti e UTF-8 malformato.
- Parser font: ordinamento, punti di codice supplementari, duplicati e capacità.
- Runtime Vulkan Intel UHD 620 con Validation Layers e sync validation: tre
  resize, VSync off/on, uscita regolare; anche starter e probe GLFW separato PASS.
  Il probe nativo verifica il percorso eventi con ASCII; l'inserimento Unicode
  dell'editor è verificato con callback simulate. La scena Vulkan mostra testo
  UTF-8 preimpostato, non una prova di digitazione Unicode dal layout del desktop.
- Cattura finale ispezionata: accenti ed euro leggibili, glifi assenti sostituiti.

Log e catture: [evidence/utf8](../evidence/utf8/). Nessuna verifica su altri
OS/driver, perdita reale della GPU o LeakSanitizer.
