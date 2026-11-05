# Verifica runtime dopo le correzioni — 26 ottobre 2026

**Verifica più recente:** [RUNTIME_FINAL.md](RUNTIME_FINAL.md) riporta il PASS
delle prove dopo le correzioni del loop, senza automazione del mouse. Questo
documento conserva gli esiti storici della precedente sessione interattiva.

**Aggiornamento successivo:** il caso di perdita di eventi è stato isolato
senza mouse/focus del desktop e corretto. Vedi [INPUT_EVENT_FIX.md](INPUT_EVENT_FIX.md).
L'utente ha confermato interazioni col mouse durante le prove originali:
non si attribuiscono quindi tutte le anomalie sotto al codice. Il report
seguente conserva gli esiti storici precedenti alla correzione.

**Esito: backend Vulkan senza diagnostiche di validazione nelle prove completate;
input testuale non ancora affidabile quando pressione e rilascio cadono tra due
frame.** Non si dichiara un PASS complessivo dell'applicazione.

Linux/X11, i3, Intel UHD Graphics 620 (WHL GT2). Esecuzione da `/tmp`, risorse
risolte nella root di questo progetto. Validation layer Khronos attivo, con
`VK_LAYER_VALIDATE_SYNC=1`; `NO_VALIDATION` rimosso dall'ambiente del figlio.

| Controllo | Esito | Evidenza |
| --- | --- | --- |
| Avvio e shutdown ordinato, scena di regressione | PASS, exit 0 nel secondo giro completo | `second-run-report.json`, `second-run-runtime.log` |
| Demo originale senza modifiche alla composizione | PASS, exit 0 e nessuna diagnostica Vulkan | `starter.log`, `starter.png` |
| Validation Vulkan e sincronizzazione | Nessun warning, errore, VUID o SYNC-HAZARD nel secondo giro completo e nel giro diagnostico | `second-run-runtime.log`, `runtime.log` |
| Resize reale | PASS: 700×820, 1100×700, 1000×800 | `second-run-report.json` |
| VSync disattivato/riattivato | Entrambe le impostazioni esercitate, swapchain ricreata senza diagnostiche | `second-run-runtime.log` |
| Nascondimento e ripristino | PASS: scratchpad i3, assenza/presenza tra le finestre visibili, rendering ripristinato | `second-run-report.json`, `restored.png` |
| Minimizzazione vera | NON VERIFICATA: richiesta inviata, ma i3 ha mantenuto WM_STATE Normal; nessuna callback iconify | `second-run-report.json` |
| Campo float | PASS: commit 0.75 | `second-run-runtime.log` |
| Campo int senza passaggio in float | PASS: commit esatto 16777217 | `second-run-runtime.log` |
| Picker directory | PASS: apertura, rendering, conferma del percorso canonico assets/fonts | `second-run-runtime.log`, `picker.png`, `picker-confirmed.png` |
| Testo e rilascio di Enter fuori focus | Due commit corretti nel primo giro; perdita di caratteri nel secondo | `first-run-runtime.log`, `second-run-runtime.log` |
| Pressione breve durante uno stallo | FAIL riproducibile: callback GLFW riceve press/release di Z, widget non inserisce Z | `runtime.log`, `short-key-after-stall.png` |

Tutte le evidenze sono in
[`evidence/runtime-validation`](../evidence/runtime-validation/).
Il riepilogo strutturato è [`summary.json`](../evidence/runtime-validation/summary.json).
Le immagini sono catture della swapchain, non screenshot del desktop.
Sono state osservate le catture `edited` del primo giro e `picker`, `restored`,
`picker-confirmed` del secondo. `edited.png` corrente appartiene invece al giro
diagnostico interferito: non va usata come prova del primo risultato.

## Difetto confermato nell'input

`glfwPollEvents()` elabora gli eventi, poi `ui_keyboard_begin_frame()` legge
lo stato finale tramite `glfwGetKey`. Se un tasto viene premuto e rilasciato
prima della lettura, il widget vede soltanto RELEASE. La correzione precedente
ha risolto i lock non aggiornati fuori focus, ma non questa perdita di eventi.

Nel secondo giro la sequenza richiesta `base pronta` diventa `base pra`.
Per isolare il caso, il giro diagnostico sospende brevemente **solo il processo
figlio**, invia press/release di Z, quindi lo riprende. Una callback di sola
osservazione nella scena stampa `key=90 action=1` e `key=90 action=0`; il testo
prima e dopo rimane `basPaecondo`. Il valore iniziale di questa prova è già
alterato da interferenze precedenti, ma la coppia di eventi Z e l'assenza
successiva del carattere sono documentate separatamente.

Il prossimo intervento sul codice applicativo deve conservare gli eventi in
ordine fino al consumo del widget, usando callback tasti/caratteri. Abilitare
solo gli sticky keys non preserva l'ordine o più pressioni dello stesso tasto.
Questa sessione aggiunge strumenti/evidenze; non applica ancora quella modifica.

## Limiti e tentativi conservati

- Il primo giro aveva un warning di configurazione del layer perché il runner
  usava `VK_LAYER_ENABLES`, deprecato nella versione installata. Corretto nel
  runner; non era un errore del renderer. Log originale conservato.
- Il giro diagnostico finale riceve eventi Super/Shift non inviati dal runner,
  cambia dimensioni/modalità della finestra e termina per timeout di resize
  con exit -15 imposto dal runner. Non è una prova di crash dell'app né un
  giro completo valido. I risultati completi precedenti restano separati.
- L'automazione usa un desktop condiviso: non è un test CI isolato. Il runner
  controlla ora il focus prima di inviare sequenze alla finestra, ma va
  eseguito senza altre interazioni sul desktop. Questa guardia finale è stata
  verificata sintatticamente, non con un ulteriore giro GUI.
- Non verificati: minimizzazione iconica, GPU AMD, altri driver/OS, LeakSanitizer,
  Unicode/IME, clipboard, picker OPEN/SAVE e device loss durante il rendering.
- Le diagnostiche d'avvio/loop non sono state convertite in questa sessione:
  i percorsi runtime `exit(1)` allora presenti sono stati rimossi successivamente
  con test headless: vedere [RENDER_ERROR_FIXES.md](RENDER_ERROR_FIXES.md).

## Riproduzione

Richiede sessione X11 con i3, xdotool, xprop e Pillow:

```sh
make BUILD=debug build/runtime-validation.out build/app-debug.out
python3 tools/runtime_validation.py
```

La scena `tests/runtime_scene.c` usa il loop, renderer e widget di produzione;
le callback aggiunte osservano gli eventi senza consumarli. La demo originale
rimane invariata. Il runner **restituisce errore** se una verifica fallisce:
la perdita del tasto breve è attualmente una regressione aperta, non viene
convertita in aspettativa di successo. I frame PPM temporanei vengono rimossi;
restano PNG selezionate e log. Il target non fa parte di `make test`.
