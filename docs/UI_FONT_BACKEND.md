# Font definiti dal backend

La UI non include più header `render/` e non legge la struttura dei font.
`text/font_types.h` dichiara soltanto `typedef struct Font Font`: i widget
conservano e passano puntatori, mentre il backend definisce i dati e li interpreta.
`TextAnchor` risiede in `text/anchor.h`; il vecchio `render/text_anchor.h`
resta un include di compatibilità.

## Migrazione dei backend

La tabella `UIRenderer` richiede una nuova callback:

```c
float (*font_ascent)(void *user, Font *font, float scale);
```

Restituisce la distanza dalla baseline alla sommità del testo, già moltiplicata
per `scale`, nelle unità del canvas UI. Deve essere finita e non negativa per
font e scale validi. Come la misura della larghezza, è sincrona e può essere
chiamata più volte durante il layout, anche prima di emettere comandi di disegno.
La draw list la inoltra immediatamente al backend, senza accodarla.
Una tabella senza questa callback viene rifiutata sia dal contesto sia dalla
creazione della draw list. Backend e consumatori vanno ricompilati.

Il backend di produzione restituisce `font->ascender * scale`, preservando le
metriche MSDF. Chi costruisce o carica il font di produzione deve includere
esplicitamente `render/font.h`: gli header UI non lo includono più indirettamente.
Le firme dei widget con `Font*` restano uguali.

Un backend indipendente può definire, ad esempio:

```c
#include "ui/ui.h"
struct Font { float baseline; };
static float my_ascent(void *user, Font *font, float scale) {
    (void)user;
    return font->baseline * scale;
}
```

Questa è la strategia dell'esempio `examples/ui_standalone.c`. Non ridefinire
`struct Font` insieme a `render/font.h`: scegliere una rappresentazione coerente
per il backend dell'eseguibile. Non passare un font a un backend che ne attende
una rappresentazione diversa. La UI non verifica a runtime tale compatibilità.
I font restano del chiamante e devono vivere finché widget, popup o comandi
registrati li referenziano; il backend può avere ulteriori vincoli sulle risorse.

## Verifiche

I test del renderer controllano metriche scalate, uso dello stato `user` corretto
e rifiuto di backend incompleti. I test della draw list controllano che la misura
sia immediata e non aggiunga comandi. Lo standalone usa un font minimale e
confronta il disegno immediato con quello differito. Il controllo architetturale
rifiuta header `render/` nel core, nel picker e negli header UI portabili.

Le evidenze sono in [evidence/ui-font-backend](../evidence/ui-font-backend/).
Restano da verificare un secondo backend grafico reale e il rendering multiwindow;
la modifica non aggiunge shaping, IME o gestione dei cluster Unicode.

Verifiche completate: 24 test, 21 anche con ASan/UBSan; build debug, release e
headless senza warning. Vulkan con draw list: 30 frame, ridimensionamenti e
cambio VSync senza errori di validazione. LeakSanitizer e verifica visiva del
picker non eseguiti. Il grafo degli include dello standalone non contiene
header del renderer.
