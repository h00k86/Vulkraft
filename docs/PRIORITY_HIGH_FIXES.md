# Correzioni ad alta priorità — 26 ottobre 2026

Corretti i punti 1–3 di `CODE_AUDIT.md`.

- **ID UI:** `ui_scoped_id` controlla la capacità rimasta dopo ogni `snprintf`.
  Una concatenazione troppo lunga restituisce una stringa terminata e troncata,
  senza avanzare oltre il buffer. Gli ID corti mantengono la forma precedente.
  La politica di identità e troncamento degli ID lunghi, punto 9 dell'audit,
  resta un intervento separato.
- **Campo testo:** verifica che il terminatore sia entro la capacità dichiarata
  e riallinea il cursore al testo del chiamante prima di elaborare l'input.
  Il cursore viene aggiornato anche dopo il ripristino tramite Escape.
  Buffer non terminati vengono ignorati senza modificarli.
- **Reload texture:** carica una risorsa temporanea. Su errore conserva texture,
  descriptor, validità dello slot e mtime precedente, consentendo un nuovo
  tentativo anche con lo stesso timestamp. Su successo attende il device,
  aggiorna lo slot e i descriptor, poi distrugge la vecchia risorsa.
  La modalità CLAMP/REPEAT viene conservata tramite `Texture.address_mode`.
  `destroy_texture` azzera gli handle e i metadati dopo la distruzione.

Il layout privato di `Texture` è cambiato: sono state ricompilate completamente
entrambe le configurazioni debug e release.

| Controllo | Esito |
| --- | --- |
| `make -B -j4 build/app-debug.out` | PASS, nessun warning/error |
| `make -B -j4 BUILD=release build/app-release.out` | PASS, nessun warning/error |
| `make test` | PASS: checker architetturale e quattro eseguibili di test |
| `make test-sanitize` | PASS: regressioni UI e transazione texture con ASan/UBSan |
| Probe originari ad alta priorità aggiornati | PASS, log `current-*` |
| Nuovo runtime Vulkan e verifica visiva | NON ESEGUITI |
| LeakSanitizer | NON ESEGUITO, disabilitato esplicitamente nei test sanitizzati |

I test aggiunti coprono scope di varie lunghezze e profondità, attraversamento
del ring di ID, modifica esterna del testo, cursori negativi o oltre la fine,
Escape/Backspace nello stesso frame, capacità minima e stringa non terminata.
Per le texture coprono fallimento completo e parziale, conservazione della
vecchia risorsa, retry, successo, ordine di sostituzione/distruzione, sampler,
mipmap, mtime invariato e distruttore richiamato due volte.

`texture_reload_test` usa adapter deterministici per loader e GPU.
`texture_reload_failure_test` collega loader e distruttore reali e intercetta
le chiamate Vulkan: verifica un reale errore di decodifica di un file non
immagine, senza usare un device. Questi test non equivalgono alla validazione
di un driver o all'osservazione del rendering.

La compilazione del test UI sanitizzato a `-O1` emette un warning preesistente
di possibile troncamento nell'ID della palette del picker (`p%d_%d`); il test
passa senza segnalazioni ASan/UBSan. Il caso non appartiene ai tre difetti
corretti. I messaggi `ERROR` di caricamento nei test sono attesi: vengono
provocati per verificare la conservazione delle risorse.

Comandi ripetibili dalla root:

```sh
make test
make test-sanitize
make
python3 evidence/code-audit/run.py --high-only
```

I log di build e test delle correzioni sono in `evidence/priority-high-fixes/`.
I log originali dell'audit sono conservati senza sovrascrittura.
