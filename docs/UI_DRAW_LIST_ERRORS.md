# Errori della draw list

La draw list conserva la prima causa di errore fino a `ui_draw_list_begin()`.
Prima della correzione, per esempio, un'allocazione fallita poteva essere seguita
da un pop su uno stack vuoto e diventare un errore CLIP, perdendo MEMORY.
Ora le operazioni successive non sostituiscono il codice iniziale.

Quando il frame è in errore:

- i nuovi comandi di disegno non vengono accodati;
- il caricamento di una preview restituisce -1 senza chiamare il backend;
- il replay restituisce false;
- le query di metriche/dimensioni restano disponibili per completare il layout;
- il rilascio della preview resta disponibile, anche differito se vi sono immagini
  accodate o già riprodotte, per rispettare la durata delle risorse.

Dopo un errore il chiamante deve scartare l'eventuale geometria parziale del
renderer. Quando le risorse del target lo consentono, `ui_draw_list_begin()`
scarta la registrazione precedente, esegue i rilasci pendenti e azzera l'errore.
La verifica del clipping iniziale può comunque far fallire il nuovo begin.
Non chiamare begin/destroy da una callback del backend.

Questa correzione non cambia le firme o l'ABI. Non aggiunge segnalazioni agli
errori interni delle callback void del target: il renderer concreto resta
responsabile di esporre i propri errori e il chiamante di non presentare un
frame fallito. Il renderer Vulkan mantiene il controllo tramite APP.

I test iniettano un errore di allocazione del testo, poi tentano clip invalidi,
nuovi disegni e caricamenti; verificano che MEMORY resti il primo errore e che
il loader non venga chiamato. Controllano anche RESOURCE seguito da CLIP,
cleanup immediato/differito e recupero con un nuovo begin e replay valido.
Evidenze: [evidence/ui-draw-list-errors](../evidence/ui-draw-list-errors/).

Risultati: 24 test, 21 anche con ASan/UBSan; build debug, release e headless
senza warning. I percorsi di errore sono verificati con fault injection e
backend simulato. Runtime Vulkan non ripetuto; LeakSanitizer non eseguito.
