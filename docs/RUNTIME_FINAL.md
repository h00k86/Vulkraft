# Verifica runtime finale — 26 ottobre 2026

PASS per le prove eseguite dopo la propagazione degli errori di rendering.
Linux/X11/i3, GPU selezionata Intel UHD Graphics 620 (WHL GT2), build debug,
`VK_LAYER_KHRONOS_validation` attivo e `VK_LAYER_VALIDATE_SYNC=1`.

- Scena di regressione: avvio e chiusura automatica dopo 10 secondi, exit 0.
- Resize della sola finestra di prova a 700×820, 1100×700 e 1000×800,
  con controllo della geometria effettiva.
- VSync disattivato a 2 secondi e riattivato a 5 secondi tramite l'API usata dalla UI.
- Nessun errore o warning di validazione nei log della scena e dello starter.
- Starter originale: avvio e shutdown da `/tmp`, exit 0.
- 30 frame catturati; cattura finale ispezionata, testo e widget visibili correttamente.
- Probe input separato: finestra GLFW nascosta e veri eventi X11 diretti alla
  finestra; il carattere `z` viene conservato sia con press/release in frame
  separati sia nello stesso frame. Il probe usa rendering stub, non Vulkan.

Il runner non invia tasti globali, non muove il mouse e non richiede il focus.
L'apertura della finestra resta soggetta alla normale politica del window manager.
La scena cambia VSync internamente solo con `APP_RUNTIME_PASSIVE` impostata.

Il primo tentativo ha prodotto un warning per un'impostazione deprecata aggiunta
al runner (`VK_LAYER_ENABLES`). È stata rimossa e la prova completa ripetuta con
successo. I log iniziali sono conservati in `first-attempt/`, separati dall'esito finale.

Evidenze: [report.json](../evidence/runtime-final/report.json),
[runtime.log](../evidence/runtime-final/runtime.log),
[starter.log](../evidence/runtime-final/starter.log),
[input-probe.txt](../evidence/runtime-final/input-probe.txt),
[final.png](../evidence/runtime-final/final.png).

Non verificati in questo giro: perdita reale della GPU, minimizzazione iconica,
altre GPU/OS, LeakSanitizer, picker interattivo e editing completo nella scena Vulkan.
Questa prova non certifica l'assenza di qualsiasi difetto.

## Riproduzione

Richiede sessione X11/i3, xdotool, Pillow e Validation Layers installati:

```sh
make BUILD=debug build/runtime-validation.out build/app-debug.out build/glfw-key-event-probe.out
python3 tools/runtime_validation_passive.py
```

Sintassi del runner, controllo dei confini architetturali e `git diff --check`:
PASS. Nessuna modifica al codice di produzione in questo giro.
