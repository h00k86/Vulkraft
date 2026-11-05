# Correzioni a priorità media — 26 ottobre 2026

Aggiornamento runtime successivo: vedi [RUNTIME_VALIDATION.md](RUNTIME_VALIDATION.md).
Il backend ha superato le prove descritte. La perdita di input tra frame
è stata poi verificata isolatamente e corretta: [INPUT_EVENT_FIX.md](INPUT_EVENT_FIX.md). I limiti e gli esiti riportati sotto sono quelli della
sessione originale.

Interventi sui punti 4–10 di `CODE_AUDIT.md`. Le correzioni delle priorità alte
restano incluse nelle regressioni. Nessun commit effettuato.

| Punto | Modifica | Verifica mirata |
| --- | --- | --- |
| 4. Swapchain | Formato/color space scelti fra quelli esposti; extent, numero immagini, transform e alpha rispettano le capability. TRANSFER_SRC richiesto solo per capture e se supportato; capture disabilitata sui formati non gestiti. | Superfici simulate con formato alternativo, extent limitato, rotazione, alpha non opaco e capture non supportata. |
| 5. Font | Parser JSON delimitato per oggetti, indipendente dall'ordine/impaginazione; letture, allocazioni, numeri, bounds e duplicati necessari verificati. Commit del Font solo a parsing riuscito. | Font del progetto, campi riordinati, spazio senza bounds, JSON incompleto/malformato/overflow numerico; output invariato su errore. |
| 6. Tastiera | Snapshot centrale in `ui_begin_frame`: i rilasci vengono osservati anche senza widget focalizzato. Gli editor consumano le pressioni una volta per frame. | Enter, perdita focus, rilascio senza widget, nuovo focus/Enter; consumo singolo dell'evento. |
| 7. Picker | Root, directory iniziale, navigazione e selezione risolte canonicamente. Per salvataggi nuovi viene verificato il genitore esistente; symlink pendenti rifiutati. Controllo ripetuto alla restituzione del percorso. | Root `/`, `..`, symlink esterno/pendente, leaf mancante, parent mancante e buffer insufficiente. |
| 8. Avvio | `app_init`, `gfx_init` e costruttori GPU restituiscono l'esito; `APP.error` identifica codice/fase. Rollback delle risorse parziali, shutdown ripetibile, verifica atlas obbligatorio e creazioni/mapping Vulkan. | 22 fasi di fallimento simulate, due allocazioni CPU fallite, retry, doppio shutdown; errori create/type/allocate/bind/map dei buffer e map/sampler texture. |
| 9. ID UI | Capacità uniforme di 256 byte negli stati; ID oltre 255 byte e composizioni eccedenti rifiutati invece di troncati. | ID da 255 byte, distinti dopo il byte 128, lookup stabile, overflow e stack invariato su push fallito. |
| 10. Interi | Editor numerico condiviso con percorso `int` separato: formattazione e parsing decimale senza conversione intermedia in float. | INT_MIN, INT_MAX, 16777217, commit invariato, decimali e overflow rifiutati. |

## Contratti per chi usa la base

- Inizializzare `APP` a zero e verificare `app_init(app)` prima del loop.
  In caso di errore leggere `app->error.code` e `app->error.stage`.
  `app_shutdown` è valido anche dopo un avvio fallito e può essere ripetuto.
  Un secondo init su un'app attiva viene rifiutato senza distruggerla.
- I costruttori GPU che restituiscono `false` possono lasciare risorse parziali
  di proprietà del chiamante; il percorso di avvio le rilascia tramite shutdown.
  L'esempio `examples/main.c` gestisce il risultato e restituisce un exit code
  di errore anche quando la ricreazione della swapchain fallisce.
- Chiamare `ui_begin_frame` una volta dopo il polling degli eventi e prima dei
  widget. Il limite degli ID comprende separatori e scope; `ui_scoped_id`
  restituisce stringa vuota su overflow. Non effettuare pop dopo un push fallito.
- Il picker restituisce un percorso verificato al momento della selezione:
  non è una primitiva di apertura atomica resistente a modifiche concorrenti
  del filesystem. Il chiamante resta responsabile dell'apertura/scrittura.
- Il parser font accetta file fino a 4 MiB, profondità 32 e 65536 token;
  la tabella runtime mantiene il limite preesistente `MAX_GLYPHS`.

## Verifiche

Evidenze in [`evidence/priority-medium-fixes`](../evidence/priority-medium-fixes/):

- Build completa debug `make -B -j4` e build release `make BUILD=release -j4`:
  PASS, nessun warning o errore nei log.
- `make test`: PASS, checker dei confini architetturali e 11 eseguibili di test.
- `make test-sanitize`: PASS, ASan e UBSan su regressioni UI, font, filesystem,
  scelta swapchain, rollback d'avvio, buffer, upload texture e reload transazionale.
  LeakSanitizer disabilitato (`detect_leaks=0`).
- Clang Static Analyzer su 9 file (configurazione DEBUG): una segnalazione
  residua `Potential leak` in `create_swapchain`. Il ramo di successo trasferisce
  `images` al chiamante, quello di errore la libera; il test della swapchain
  verifica entrambi i fallimenti di query, il trasferimento al contesto e il
  doppio cleanup con conteggio delle allocazioni a zero. Nessuna modifica
  aggiunta per sopprimere il warning; resta visibile nel log.
- `git diff --check`: PASS.

I test d'avvio eseguono l'orchestrazione reale con costruttori backend simulati;
quelli buffer/texture eseguono i rispettivi percorsi reali con API Vulkan
intercettate. Non costituiscono una verifica di rendering su un driver.
Il runner `evidence/code-audit/run.py` richiama ora le suite permanenti;
probe e log originari rimangono evidenze storiche dei difetti.

## Limiti ancora presenti

- La correzione del punto 6 risolve i lock dei tasti. Input Unicode/IME,
  repeat e clipboard, insieme alla decodifica UTF-8 del renderer, restano
  lavoro separato: l'editor continua a ricostruire caratteri ASCII dai tasti.
- I percorsi runtime `exit(1)` ancora presenti al termine di questo intervento
  sono stati successivamente rimossi: vedere [RENDER_ERROR_FIXES.md](RENDER_ERROR_FIXES.md).
- Stato UI/texture globale e limiti fissi dei registri restano invariati.
  Non è stata introdotta una gestione di più applicazioni concorrenti.
- Nessuna nuova esecuzione con finestra, osservazione visiva, resize reale,
  validation layer GPU, matrice driver, Windows/macOS o LeakSanitizer.
  Il ramo Windows del resolver è aggiornato ma non compilato/verificato qui.
