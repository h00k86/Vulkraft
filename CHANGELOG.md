# Changelog UI Framework

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
