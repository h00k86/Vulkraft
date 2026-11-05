# Decorazioni opzionali del backend

Un backend UI non deve più implementare gli effetti grafici specifici del
renderer di origine. Può lasciare NULL una o entrambe queste callback:

- `draw_hud_paper_panel`
- `draw_hud_trash_swipe`

Nove callback rimangono obbligatorie; anche i tre servizi di
[anteprima/dimensioni immagine](UI_OPTIONAL_PREVIEWS.md) sono ora opzionali. Contesto e draw list usano
lo stesso controllo di validità. Le firme e la struttura `UIRenderer` non
cambiano in questo passaggio; i backend già completi continuano a funzionare.
Il renderer Vulkan mantiene entrambe le implementazioni originali.

## Comportamento senza callback

Il pannello viene disegnato come rettangolo con il colore e l'alpha richiesti.
Quando è focused, una striscia superiore nel colore accent segnala lo stato:
altezza massima due unità, limitata all'altezza del pannello.

Lo swipe usa una normale immagine con UV 0..1 se lo slot è non negativo.
Senza immagine usa un rettangolo nero traslucido (alpha 0.15, oppure 0.25
quando focused). Dimensioni non positive non producono primitive di fallback.
Il seed non modifica i fallback: non simulano il bordo irregolare o il rumore
degli effetti personalizzati. Le callback presenti ricevono gli argomenti originali.

L'opzione di tema `paper_panels` continua a selezionare questo percorso.
La draw list copia i parametri delle decorazioni e sceglie callback o fallback
al replay, usando le stesse funzioni del disegno immediato. I consueti vincoli
di durata delle immagini, ordine dei comandi e clipping restano applicabili.

## Verifica

Lo standalone omette entrambe le callback. Confronta le tracce complete di
primitive, testo e clip nei percorsi immediato/differito, con tema piatto e
con `paper_panels`, mantenendo input UTF-8 e interazione del pulsante.
I test controllano anche backend con una sola callback mancante, l'inoltro
alle callback presenti e lo swipe con e senza immagine.

Evidenze: [evidence/ui-optional-skin](../evidence/ui-optional-skin/).
Il fallback è verificato con backend simulati; il test Vulkan usa gli effetti
originali e non costituisce una valutazione visiva del nuovo fallback.

Risultati: 24 eseguibili di test, 21 anche con ASan/UBSan; build debug, release e
headless senza warning. Runtime Vulkan con draw list: 30 frame, ridimensionamenti
e cambio VSync senza errori di validazione. LeakSanitizer non eseguito.
