# Changelog UI Framework

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
