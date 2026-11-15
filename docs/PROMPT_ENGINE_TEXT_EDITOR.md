# Prompt per l'agente del motore

Integra nel game engine il nuovo componente multilinea di Vulkraft. Il lavoro
richiesto riguarda il pannello di modifica del testo e la sua integrazione host;
non introdurre ora Lua, C dinamico, nuovi compilatori o una IDE completa.

## Sorgente da importare

Il componente è nel checkout:
`/home/h00k/CODING/C/GAME/VULKAN_3D/GRAPHICS_APP_BASE`.
Versioni: UIFramework **0.7.0**, GraphicsApp **0.9.0**.

Questa consegna può essere ancora non committata: **non importare semplicemente
HEAD**, che potrebbe essere la versione precedente. Usa l'archivio sorgente
`build/ui-framework-0.7.0-source.tar.gz` prodotto da `tools/package_ui.py` e il
relativo manifest/checksum. Se il percorso di output cambia, chiedi il percorso
dell'artefatto o leggilo dall'output del comando. Se l'archivio non è presente,
fallo generare nel checkout Vulkraft, senza modificare i sorgenti del toolkit.
Se il lavoro viene poi committato, si può invece fissare quel commit preciso.

Aggiorna la copia vendorizzata completa, non soltanto gli header: il nuovo
`src/ui/ui_text_editor.c`, le modifiche a `ui_text_edit.c`, gli header privati e
la navigazione devono appartenere allo stesso snapshot. Aggiorna VERSIONE.md
indicando versione, provenienza e checksum; non dichiarare un commit non presente.
Ricompila tutti i consumatori. Non creare collegamenti/submodule al repo vivo.
Se l'importatore usa liste di sorgenti esplicite, aggiungi `ui_text_editor.c`
anche a Makefile/CMake del motore. Conserva gli adattatori e le personalizzazioni
host già concordate senza alterare i sorgenti originali del toolkit.

## Contratto da leggere prima di scrivere codice

`docs/UI_TEXT_EDITOR.md`, `include/ui/ui.h`,
`docs/UI_INTEGRATION_BEHAVIOR.md` dell'archivio.

Chiamata:
```c
UITextEditorOptions options = {
    .scale = scala_del_font_del_motore,
    .line_numbers = true,
    .highlight_line = riga_errore,  /* 0 disabilita */
    .reveal_line = riga_da_aprire   /* impulso; azzerare dopo la chiamata */
};
UITextEditorResult r = ui_text_editor(id_stabile, font, rettangolo,
                                      documento.testo, documento.capacita, &options);
```

Il buffer è dell'host, UTF-8 valido, terminato NUL, LF, capacità massima **32768
byte incluso NUL**. Il caricatore deve normalizzare CRLF/CR e rifiutare file
binari, NUL incorporati, UTF-8 invalido o troppo grande: nessun troncamento
silenzioso. Se il documento cambia dall'esterno, l'host decide quando sostituirlo;
la sostituzione del contenuto invalida la storia del widget.

`changed` indica una differenza nel frame, non «sporco per sempre»: confronta il
testo con la versione salvata anche dopo undo/redo. `save_requested` indica
Ctrl+S e non scrive nulla. Esc lascia l'editing senza ripristinare il documento.
Invio inserisce LF; Tab inserisce un tab; Shift+Tab esce. Posizioni cursore in
byte, righe/colonne da 1. Per focus e cattura usa le API pubbliche esistenti,
senza leggere ID interni o strutture private. L'undo è del documento di testo,
separato dalla history degli oggetti della scena.

## Implementazione richiesta nel motore

1. Prima verifica il ramo, le modifiche presenti e l'import attuale di Vulkraft.
   Preserva modifiche estranee. Lavora in un commit/ramo dedicato; non fare merge
   o push se non già autorizzati dall'utente per questa attività.
2. Aggiungi un pannello «Sorgente» accessibile dall'editor. Parti da un documento
   temporaneo con più righe, tab e accenti, senza toccare file di gioco reali.
   Usa un ID stabile per documento, una dimensione adattabile alla finestra e
   lo stesso font/tema dell'editor. Mostra riga/colonna e stato modificato.
3. Aggiungi Apri, Salva e chiusura del documento usando i servizi host/picker già
   presenti. Per la prima versione basta un documento aperto alla volta.
   Apri solo sorgenti/testi previsti (.vert, .frag, .comp, .glsl, .c, .h, .txt),
   rispettando le radici/percorso consentiti dall'editor. .glb resta fuori.
   Prima di sostituire/chiudere un documento modificato, usa la conferma esistente.
   Un errore di scrittura mantiene testo e stato modificato; aggiorna la copia
   salvata solo dopo esito positivo. Rileva cambiamenti esterni prima di
   sovrascrivere e chiedi quale versione mantenere.
4. Integra correttamente l'input: regioni registrate prima dell'update del motore,
   comandi della scena sospesi durante la cattura, clipboard dell'host. Ctrl+Z/Y
   nel sorgente non devono modificare oggetti e Invio non deve chiudere il pannello.
5. Per uno shader salvato usa soltanto il meccanismo di hot reload già esistente,
   senza costruire adesso una nuova pipeline di compilazione. Un file C salvato
   è soltanto testo e non viene compilato/eseguito dal motore. Il pulsante
   «Compila e applica», compilazione asincrona e diagnostica strutturata sono la
   fase successiva, da concordare separatamente.

## Collaudo e consegna

- Scenario con `type`: accenti, Invio, Tab, selezione, copia/incolla, undo/redo
  locale, Ctrl+S e riapertura. La chiusura con modifiche deve poter essere annullata.
- Verifica che G/R e Ctrl+Z della scena non scattino durante l'editing e tornino
  disponibili quando si esce. Prova cambio focus, dropdown/modale sopra il
  pannello, ridimensionamento e finestra stretta (956 px).
- Prova un errore di salvataggio senza perdere il documento, UTF-8 invalido,
  CRLF e documento troppo grande. Controlla lo scroll su molte righe e riga lunga.
- Usa file temporanei per i test: nessuna riscrittura di shader/asset reali solo
  per dimostrare che il pannello funziona. Eventuali prove del reload reale vanno
  eseguite in una fixture e ripristinate, con esiti riportati separatamente.
- Esegui suite del motore, collaudo editor e build Make/CMake appropriate.
  Distingui prove automatiche, vista a schermo e tastiera reale: `type` non
  dimostra la digitazione del sistema operativo. Non dichiarare prove non svolte.
- Consegna commit, versione/checksum Vulkraft importati, risultati e limiti.
  Segnala eventuali difetti del componente al suo agente; non correggere in modo
  divergente la copia vendorizzata senza rendere esplicita la patch.
