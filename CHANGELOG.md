# Changelog UI Framework

## 0.6.0 — eventi e accessori delle righe

- `UICollectionResult.clicked_index` segnala anche clic su righe già selezionate,
  distinguendoli da selezione, espansione e attivazione da tastiera.
- `ui_tree_ex` aggiunge accessori di riga con icona, testo facoltativo, tooltip,
  selected/disabled e navigazione Tab. `accessory_index` segnala l'azione
  senza cambiare selezione, espansione o attivazione del nodo.
- Icone geometriche `UI_ICON_MOVE`, `UI_ICON_ROTATE`, `UI_ICON_SCALE`;
  l'icona texture conserva il suo valore e supporta gli asset dell'host.
- Alfa compatta con integer_channels: `1`, `0.6`, mantenendo precisione e frazioni.
- Demo aggiornata con azioni occhio/lucchetto; test per mouse, tastiera,
  clic ripetuti, annullamento del click e spazio riservato ai bottoni.
- GraphicsApp 0.8.0 richiede UIFramework 0.6.0. Ricompilare l'host per la nuova
  dimensione di UICollectionResult; le chiamate esistenti restano valide.

## 0.5.0 — campi, icone, liste e colore RGBA/HSV

- `ui_text_input_ex` con segnaposto disegnato separatamente dal buffer e dalla
  cronologia; le chiamate precedenti e i campi password restano compatibili.
- `ui_icon_button` con icone geometriche integrate o texture dell'host, testo
  facoltativo, stati selected/disabled e interazione standard mouse/tastiera.
- `ui_list` e `ui_tree` con dati/espansione dell'host, selezione singola,
  scorrimento e navigazione; risultati distinti per selezione, espansione,
  attivazione. Disegno limitato alle righe visibili.
- `UIColorRGBA`, conversioni HSV e modalità HSV nelle opzioni del picker:
  riquadro SV, tonalità, H/S/V e alfa indipendente con anteprima a scacchiera.
  `UIColor` e il picker RGB precedente mantengono la loro API. HDR separato.
- Memoria HSV per contesto/ID con cache riutilizzabile; diagnostica
  `UI_OPERATION_COLOR_PICKER` su esaurimento. Nessuna dipendenza dal backend.
- Demo condivisa, test di interazione e confronto pixel SDL immediato/differito.
- GraphicsApp 0.7.0 richiede UIFramework 0.5.0. Ricompilare host e librerie:
  `UIColorPickerOptions` ha un nuovo membro `mode` (zero-inizializzare).

## 0.4.0 — integrazione colore e tab

- Nuova `ui_property_color_ex`: opzioni facoltative e segnale pubblico
  `out_drag_started`, senza dipendere dagli ID dei controlli RGB interni.
  La funzione precedente resta disponibile con il comportamento preesistente.
- `UIColorPickerOptions.integer_channels` arrotonda soltanto il testo dei canali
  a riposo; editing preciso e colore originale non vengono modificati dal disegno.
- Tab con etichette accorciate mediante `...`, misurate dal renderer e ritagliate
  sul bordo del tab, senza spezzare UTF-8 o cambiare selezione/etichette host.
- Esempio Opzioni aggiornato e regressioni su gesti, tastiera, precisione del
  colore, campi attivi, tab stretti e testo Unicode/lungo.
- GraphicsApp 0.6.0 richiede UIFramework 0.4.0; ricompilazione necessaria.

## 0.3.1 — rilascio della tastiera e numeri compatti

- Il focus lasciato da un clic non cattura la tastiera; Tab e la navigazione
  esplicita continuano a farlo. Testo/numeri in editing e overlay catturano
  sempre, indipendentemente da come sono stati attivati.
- Invio/Esc terminano l'editing e rilasciano la cattura conservando la posizione
  per Tab. Una scorciatoia successiva non riapre il campo. Click, Tab, focus
  programmato e Invio consentono di riprendere l'editing.
- Numeri float compatti con precisione sufficiente a ricostruire lo stesso
  valore, senza modificare il modello. Testo numerico ritagliato al campo;
  durante l'editing resta visibile la parte finale. Ammessi esponenti e `+`
  nell'editing float, con la stessa validazione alla conferma.
- GraphicsApp 0.5.1 richiede UIFramework 0.3.1; ricompilare host e librerie.

## 0.3.0 — sigle nelle etichette

- Nuova `ui_set_sentence_case_exceptions`: elenco facoltativo per contesto di
  parole ASCII da conservare con sentence case attivo, senza vocabolario
  incorporato o allocazioni; array e stringhe sono presi in prestito.
- Confronto esatto per parola, sensibile alle maiuscole, anche in presenza
  di punteggiatura; reset e sostituzione eliminano la configurazione precedente.
- Test su testo disegnato, parole composte, confini UTF-8, troncamento,
  input non valido e isolamento dei contesti.
- GraphicsApp 0.5.0 richiede UIFramework 0.3.0; ricompilazione necessaria.

## 0.2.0 — comportamento di integrazione UI

- Etichette conservate come fornite; nuova opzione per contesto
  `ui_set_sentence_case`, disattivata per default.
- Esc chiude direttamente la modale attiva anche con un editor focalizzato;
  popup e menu conservano la priorità e consumano l'evento.
- Anello di focus solo per navigazione da tastiera: autofocus, ripristino
  e sostituzione di controlli non promuovono il mouse a tastiera.
- Prove permanenti per input misto, ripristino focus, annullamento modale,
  etichette effettivamente disegnate e isolamento/reset dei contesti.
- GraphicsApp 0.4.0 richiede UIFramework 0.2.0; ricompilazione necessaria.

## 0.1.0 — versione di riferimento, 4 novembre 2026

Prima base documentata del pacchetto UI standalone. Le copie di sviluppo che
riportavano già 0.1.0 non identificano necessariamente gli stessi sorgenti:
per questa versione fanno fede manifest e SHA-256 del pacchetto consegnato.

- Core C11 indipendente da GLFW/SDL/Vulkan, contesto opaco, input esplicito,
  renderer a callback e draw list differita.
- Widget di base, layout/proprietà, tab/splitter, menu gerarchici, dialoghi,
  color picker, editing per scalari Unicode UTF-8, clipboard dell'host e undo/redo.
- Picker filesystem e backend SDL2 opzionali, selezionabili tramite componenti
  CMake; esempio editor con salvataggio atomico e rilevamento dei conflitti.
- Riserve dinamiche riutilizzabili per testi differiti, owner degli eventi,
  opzioni dropdown e voci menu; diagnostica tipizzata nei punti documentati.
- Test di guasto e sessioni prolungate sul core e sull'editor reale.
- Versione centralizzata in `include/ui/version.h`, macro `UI_VERSION_*` e
  funzione `ui_version_string()` per interrogare il core collegato.
- Archivio sorgente normalizzato, manifest SHA-256, checksum dell'archivio e
  riepilogo delle verifiche associato all'esatto archivio controllato.

Compatibilità, limiti e aggiornamenti: `UI_COMPATIBILITY.md`.
Riproduzione del pacchetto: `UI_RELEASE.md`. Nel repository i due documenti
sono in `docs/`; nell’installazione sono accanto a questo changelog.
