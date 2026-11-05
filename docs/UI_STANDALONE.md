# Esempio standalone con API pubbliche

`examples/ui_standalone.c` usa solo gli header pubblici della UI e un backend
simulato. Non include sorgenti `.c` o header interni, non crea un'istanza APP e
non richiede finestra, GPU, font su disco o shader compilati.
I widget non ricevono APP*: l'esempio collega esplicitamente il proprio backend
al contesto. Vedi la [migrazione delle firme](UI_WIDGET_API.md).

## Compilazione ed esecuzione

Dalla radice del progetto:

```sh
make ui-standalone
./build/ui-standalone-debug.out
make BUILD=release test-ui-standalone
make test-ui-standalone-sanitize
```

Per questi target servono compilatore C11, Make, `ar` e libreria matematica;
GLFW, Vulkan e `glslc` non vengono usati. I target non dipendono dall'applicazione
grafica predefinita. Sono disponibili anche le librerie `build/libui_debug.a` e
`build/libui_release.a`, collegate dall'esempio come un normale consumatore.

Il collegamento non usa `--gc-sections` e i sorgenti vengono compilati in unità
separate. Non ci sono stub per soddisfare simboli mancanti del framework:
le callback simulate sono l'implementazione dell'interfaccia pubblica UIRenderer.

## Cosa dimostra

L'esempio esegue la stessa sequenza su due contesti creati separatamente:

- cinque frame di input esplicito, senza polling;
- inserimento UTF-8 di `Caffè €` in un campo con focus;
- pressione e rilascio del mouse su un pulsante, con una sola attivazione;
- disegno attraverso un backend immediato e attraverso una draw list;
- clipping bilanciato e rilascio ordinato di contesto e lista.

Il backend conta le operazioni e calcola una firma di tipi, coordinate, colori e
testi. Verifica che le due modalità producano la stessa sequenza di disegno e
che il backend differito non riceva comandi prima del replay. Misura il testo
con metriche monospazio simulate per punti di codice Unicode.
I controlli usano il valore di ritorno del programma e rimangono attivi in release.

Output atteso:

```text
Standalone UI: PASS
text="Caffè €" clicks=1 frames=5 per mode
immediate/deferred: identical trace (42 operations, 10 texts, 5 clips)
```

La simulazione non genera pixel e non verifica la qualità visiva, il font MSDF,
le texture o il comportamento del driver GPU. È un esempio eseguibile del
contratto pubblico, non un secondo renderer grafico.

## Dipendenze residue e verifiche

La libreria UI contiene ora soltanto core e draw list. Il picker e gli
adattatori dei percorsi sono in [un modulo opzionale](UI_FILE_PICKER.md), da
collegare solo quando necessario. L'esempio non include né collega quel modulo.
L’esempio definisce un proprio font minimale, senza atlante o glifi MSDF:
vedi [font del backend](UI_FONT_BACKEND.md). I codici portabili dei tasti restano
nel contratto.
Le evidenze iniziali in ui-standalone precedono questa separazione; per l'assenza
di riferimenti filesystem vedere anche evidence/ui-picker-module.

`make test` e `make test-sanitize` includono l'esempio. Il controllo architetturale
impedisce l'uso diretto di APP o di header/sorgenti privati nel consumatore.
Le evidenze in [evidence/ui-standalone](../evidence/ui-standalone/) comprendono
build debug/release, sanitizzatori, grafo degli include del compilatore e simboli
collegati. Su Linux `ldd` mostra soltanto le librerie di sistema, senza GLFW/Vulkan.
Non viene certificata da questo esempio la portabilità su altri sistemi operativi.

L’esempio omette le due callback di decorazione e verifica due temi (piatto e
`paper_panels` con fallback). Per entrambi confronta la traccia completa tra
disegno immediato e differito: [decorazioni opzionali](UI_OPTIONAL_SKIN.md).

Lo standalone omette anche le tre callback dei servizi anteprima/dimensioni
immagine: non sono più necessari stub per funzioni che l’esempio non usa.
