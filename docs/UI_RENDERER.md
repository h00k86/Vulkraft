# Backend di rendering UI

`UIRenderer` descrive le operazioni usate dai widget: rettangoli RGB/RGBA,
testo e misura, immagini, pannelli decorativi, clipping e anteprime del picker.
Ogni callback riceve un `void *user` del backend, senza `APP*` o tipi Vulkan.
Il core non chiama più il renderer concreto o il texture manager e non include
i loro header. Il controllo architetturale protegge questo confine.

## Collegamento e proprietà

```c
UIContext *ctx = ui_context_create();
/* Implementare le callback obbligatorie di ui/renderer.h. */
UIRenderer backend = my_renderer_callbacks();
if (!ctx || !ui_context_set_renderer(ctx, &backend, my_renderer_state)) {
    ui_context_destroy(ctx);
    /* Gestire l'errore. */
}
```

La tabella viene copiata. Lo stato `user`, i font e le risorse restano del
chiamante e devono vivere finché il contesto li usa. Nove callback sono
obbligatorie: una tabella incompleta viene rifiutata senza cambiare il backend.
Le due callback di decorazione `draw_hud_paper_panel` e `draw_hud_trash_swipe`
sono [opzionali, con fallback portatile](UI_OPTIONAL_SKIN.md).
I servizi di dimensione immagine e anteprima sono [opzionali](UI_OPTIONAL_PREVIEWS.md).
Un backend senza anteprime può omettere le tre callback relative, senza stub.

`ui_context_set_renderer(ctx, NULL, NULL)` scollega il backend. Contesto NULL o
frame aperto fanno restituire false. Reset conserva backend e `user`, ma azzera
lo stato dei widget. Distruzione non distrugge il backend del chiamante.
Collegare un backend prima di usare funzioni che disegnano, misurano testo o
eseguono clipping; in debug una chiamata senza backend causa un'asserzione.
Un contesto senza backend può comunque essere usato per operazioni di solo stato.

Sostituzione, scollegamento, reset e distruzione liberano l'anteprima posseduta
tramite il vecchio backend e il suo `user`. Anche il cambio di contesto corrente
continua a rilasciare l'anteprima: l'adattatore di produzione usa uno slot
transiente condiviso. Il picker può ricaricarla al prossimo disegno.

Le callback vengono chiamate sincronicamente. Stringhe e array sono validi
solo durante la callback: un backend che accoda comandi deve copiarli e gestire
la durata di font e texture. Le callback non devono cambiare o distruggere il
contesto. Il backend implementa lo stack di clipping e la sua intersezione.
Completare il submit della geometria prima di cambiare contesto o backend.
Tutto resta sul thread principale.

## Adattatore esistente

`src/render/ui_renderer.c` traduce le callback nelle funzioni del renderer
attuale. `ui_bind_app_renderer(APP*)`, dichiarata in `ui/app_adapter.h`, collega
il contesto posseduto dall'app al suo renderer. `app_init()` esegue questo
passaggio automaticamente; `app_shutdown()` conserva l'ordine UI, grafica,
finestra. L'esempio non richiede modifiche.

I widget usano il backend del contesto selezionato e non ricevono più `APP*`.
Ogni backend riceve il proprio `user`, anche con gli stessi ID in contesti diversi.
La [guida di migrazione](UI_WIDGET_API.md) descrive le nuove firme.

## Verifiche e confini rimasti

`tests/ui_renderer_test.c` contiene un backend completo di registrazione e
chiama pulsanti e dialoghi reali senza definire APP o collegare un renderer.
Controlla testo UTF-8, misura, alpha, clipping, immagini, copia della tabella,
isolamento dei backend e rilascio delle anteprime con il vecchio `user`.
I test preesistenti usano un adattatore di disegno simulato.

Le evidenze di build debug/release/headless, suite ordinaria, ASan/UBSan e scena
Vulkan sono in [evidence/ui-renderer](../evidence/ui-renderer/). Il file
`core-symbols.txt` conferma l'assenza di riferimenti del core a disegno concreto,
collector, finestra e Vulkan. Le funzioni filesystem del picker sono ora in
[un modulo opzionale](UI_FILE_PICKER.md), assente dalla libreria core.

Le callback immediate possono essere avvolte nella [draw list opzionale](UI_DRAW_LIST.md),
che copia i dati temporanei e permette un replay differito per frame.
I font sono ora [opachi per la UI](UI_FONT_BACKEND.md): le metriche di layout
passano dalle callback. Gli handle immagine restano interi. Il renderer Vulkan mantiene clipping, canvas e cache
texture condivisi: non è ancora supportato il rendering simultaneo di più
finestre. Un secondo backend grafico reale non è stato implementato o validato.

Push/pop del clipping falliti devono lasciare invariato lo stack del backend.
I contenitori espongono l’esito del push: [contratto degli scope](UI_SCOPE_CLIPS.md).
