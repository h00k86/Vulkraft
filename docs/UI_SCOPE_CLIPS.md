# Clipping dei contenitori

Pannelli e aree scroll ricordano se hanno acquisito il proprio clip. In precedenza,
la chiusura eseguiva un pop anche dopo un push rifiutato: questo poteva rimuovere
il clip del contenitore padre. Una seconda chiusura dell'area scroll poteva
rimuovere un ulteriore livello.

`UIPanel` e `UIScrollScope` espongono ora `clip_pushed`. Controllarlo prima di
emettere il contenuto, poi chiamare comunque la funzione di chiusura:

```c
UIScrollScope scope = ui_scroll_begin("content", viewport, 1.0f);
if (scope.clip_pushed) {
    /* Comporre i widget usando scope.layout. */
}
ui_scroll_end(&scope);
```

`ui_panel_end()` e `ui_scroll_end()` rilasciano solo il clip acquisito e azzerano
`open` e `clip_pushed`. Chiamarli di nuovo sullo stesso oggetto non fa nulla.
Anche oggetti inizializzati a zero e puntatori NULL sono accettati dalla chiusura.
Gli scope vanno chiusi in ordine inverso all'apertura, nello stesso contesto e
frame. Non copiare uno scope attivo e chiudere entrambe le copie: questi campi
non sono token globali e non possono rilevare copie o chiusure fuori ordine.

Se il push fallisce, l'area scroll può aggiornare le proprie metriche ma non
disegna la scrollbar. Il pannello può aver già emesso sfondo e intestazione
prima del tentativo di clip. Il chiamante deve saltare il contenuto: starter e
picker ora applicano questo controllo. Il picker resta aperto e può riprovare
al frame successivo; non conferma una selezione attraverso controlli non disegnati.

`ui_clip_push()` rifiuta rettangoli con coordinate/dimensioni non finite o
dimensioni non positive senza invocare il backend. Per contratto, un push/pop
che restituisce false deve lasciare invariato lo stack del backend. Se un pop
fallisce, questa modifica non ripristina automaticamente uno stack guasto.

Le strutture pubbliche cambiano dimensione: ricompilare librerie e consumatori.
Usare le funzioni begin per inizializzare gli scope, senza costruirli manualmente
come se fossero già aperti.

I test coprono scope annidati, push rifiutati, contenuto vuoto, doppia chiusura,
rettangoli non finiti, replay differito e picker con capacità di clipping esaurita.
Evidenze: [evidence/ui-scope-clips](../evidence/ui-scope-clips/).

Verifiche completate: 24 test, 21 anche con ASan/UBSan; build debug, release e
headless senza warning. Vulkan con draw list: 30 frame, resize, cambio VSync e
starter senza errori di validazione. LeakSanitizer e picker interattivo Vulkan
non verificati.
