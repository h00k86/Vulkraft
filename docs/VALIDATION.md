# Verifiche dell'estrazione

Per il framework attuale usare [PROJECT_VALIDATION.md](PROJECT_VALIDATION.md):
`make validate`, `make validate-native` e benchmark ripetibile. Il contenuto
seguente documenta la sessione storica di estrazione.

Aggiornamento runtime successivo: vedi [RUNTIME_VALIDATION.md](RUNTIME_VALIDATION.md).
Il backend ha superato le prove descritte. La perdita di input tra frame
è stata poi verificata isolatamente e corretta: [INPUT_EVENT_FIX.md](INPUT_EVENT_FIX.md). I limiti e gli esiti riportati sotto sono quelli della
sessione originale.

Eseguite il 26 ottobre 2026 su Linux, usando il codice corrente del workspace
ENGINE (comprese le sue modifiche locali), senza compilare o scrivere nel
checkout originale.

| Controllo | Esito | Evidenza |
| --- | --- | --- |
| Build debug C11 e shader GLSL | PASS | `evidence/build-debug.log` |
| Build release separata | PASS | `evidence/build-release.log` |
| Test UI di input, ownership, scope e stato riusato dal motore | PASS | `evidence/tests.log` |
| Confine Vulkan e assenza di moduli di gioco | PASS | `evidence/extraction-audit.log` |
| Provenienza di tutte le definizioni C/GLSL estratte | PASS, 431 definizioni, nessuna senza origine | `docs/extraction-manifest.json` |
| Avvio reale Vulkan da `/tmp`, quindi fuori dalla root dell'applicazione | PASS, uscita 0 | `evidence/runtime-smoke.json` |
| Validation layer Vulkan attivo | PASS, nessun errore/VUID nei log | `evidence/runtime.log` |
| Click, testo e cambio tema osservati nella cattura del renderer | PASS: `Click: 2`, `base pronta`, tema chiaro | `evidence/app-tall.png` |
| Resize reale 1000x680 -> 700x820 | PASS | `evidence/runtime-smoke.json`, `evidence/runtime.log` |
| Indipendenza dal checkout ENGINE | PASS: asset locali, libreria autonoma, nessun include o link al gioco | checker, avvio fuori root |
| Integrita' del checkout di partenza | PASS: 525 hash invariati e stesso stato Git | `evidence/source-integrity.json` |

La prova runtime ha catturato 36 fotogrammi della swapchain. Le PNG conservate
sono catture Vulkan dell'applicazione, non immagini illustrative o screenshot
dell'intero desktop. L'automazione agisce solo sulla finestra del processo che
avvia: la rende flottante con i3, invia input e la ridimensiona.

Non vengono dichiarati verificati: runtime Windows/macOS, input IME/Unicode
completo, un editor multilinea, un'applicazione calcolatrice completa,
LeakSanitizer o un secondo backend grafico. La prova non scrive configurazioni,
progressi o salvataggi del motore; il nuovo picker usa un'identita' utente
separata quando la persistenza e' richiesta dall'applicazione.

Le modifiche rispetto all'engine sono adattamenti dentro funzioni gia'
esistenti: riduzione delle strutture, rimozione dei passaggi di gioco,
proiezione della UI su tutta la finestra, rinomina del ciclo applicativo e
composizione dimostrativa di widget gia' disponibili. Il manifesto riporta i
nomi originali anche per le cinque funzioni rinominate. Il corpo delle funzioni
adattate non viene presentato come una copia identica del sorgente originale.
