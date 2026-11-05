# Backend SDL2 software e demo di riferimento

```sh
make ui-reference-sdl2
./build/ui-reference-sdl2-debug.out
# Alternativa: disegno immediato
./build/ui-reference-sdl2-debug.out --immediate
make ui-reference-vulkan
./build/ui-reference-vulkan-debug.out
```

Dipendenze opzionali: SDL2 >= 2.0.18, SDL2_image, FreeType, pkg-config. La macchina
di verifica usa SDL2 2.32.74 tramite sdl2-compat/SDL3; non è una verifica separata
su SDL2 nativo. Il percorso di disegno usa `SDL_CreateSoftwareRenderer` su una
superficie RGBA, poi copia i pixel alla superficie finestra. Non crea un device
Vulkan né un contesto OpenGL. Le librerie di sistema SDL possono avere ulteriori
dipendenze/transizioni interne; non fanno parte delle dipendenze del core UI.

La scena comune è `examples/ui_reference_scene.c`: testo UTF-8, checkbox, slider,
dropdown, tema, pulsante con contatore, dialogo modale, scroll e picker con preview.
SDL e Vulkan la compilano senza modifiche, ciascuno con i propri font e renderer.
Il font SDL è OpenSans Italic del repository, con licenza OFL allegata; le metriche
sono diverse dai font MSDF, quindi non si richiede uguaglianza di pixel tra backend.

`--font percorso.ttf` e `--root directory` rendono esplicite le risorse quando si
avvia fuori dalla directory del progetto. `--smoke` esegue 30 frame, inserisce
UTF-8 nell'adattatore (senza input al desktop) e ridimensiona due volte la finestra.
`--navigation-smoke` verifica 30 frame della navigazione da tastiera e lo stato
finale dei controlli. Funziona anche con `--immediate`; non invia input al desktop.
`--editing-smoke` verifica selezione UTF-8 e undo/redo senza usare la clipboard
del desktop. La demo collega il servizio clipboard con `ui_sdl2_bind_clipboard()`;
contratto e limiti in [UI_TEXT_EDITING.md](UI_TEXT_EDITING.md).
`--dump file.bmp` salva la superficie finale; `--frames N` limita una sessione.

## Contratto e limiti

Il backend possiede superficie, font FreeType e cache limitata a 512 glifi.
Le dimensioni font supportate sono 1–256 pixel (arrotondate); usa kerning FreeType
quando disponibile, fallback U+FFFD/`?`, senza shaping. Colori RGB UI convertiti
dallo spazio lineare con convenzione gamma 2.2; alpha lineare. SDL effettua il
blending sulla superficie codificata: non si promette identità con il blending
lineare Vulkan. Il clipping annidato conserva esplicitamente le intersezioni vuote.

Le immagini usano per ora un solo slot, 0, per la preview. Il loader mantiene
la superficie caricata fino a sostituzione/rilascio; stesso percorso riutilizzato
senza hot reload. Il resize ricrea renderer e texture, conservando le superfici.
Dimensione massima della superficie: 8192 per lato. Ridimensionare solo tra frame,
dopo submit/discard e prima del nuovo begin. Il puntatore alla superficie cambia.

L'host possiede SDL_Init/SDL_Quit e finestra; una istanza backend possiede un font.
Chiamare begin/end del backend e verificare `ui_sdl2_error`. Un secondo begin o un
resize durante un frame (anche a dimensioni invariate) viene rifiutato senza
modificare il frame attivo. End senza begin viene rifiutato. Un end fallito lascia
il frame attivo: si possono bilanciare i clip mancanti oppure chiamare
`ui_sdl2_discard`, che chiude il frame e azzera i clip senza presentarlo, conservando
l'errore fino al prossimo begin. La superficie scartata non va copiata alla finestra.
Se si usa una draw list ancora pendente, chiamare poi `ui_draw_list_discard(list)`
prima del resize: prima si scarta il target, poi si liberano le risorse della lista.
Distruggere prima contesto UI e draw list, poi backend e finestra. Il font SDL non
è intercambiabile con il `Font` MSDF. Solo una finestra attiva nella demo; rendering
multiwindow e altre piattaforme non sono verificati.

L'adattatore traduce scancode, testo UTF-8, repeat, modificatori, mouse, rotella e
perdita del focus. Dopo `FOCUS_LOST` scarta l'input fino a `FOCUS_GAINED`;
se perdita e recupero avvengono nello stesso batch, quel batch rimane annullato
e l'input riprende dal successivo. Inizializzare `UISDL2Input` a zero prima dell'uso.
Overflow del batch tastiera rifiuta il frame; non tronca silenziosamente.
L'host filtra per windowID e legge tutti gli eventi prima del frame. L'adattatore
conserva fino a 128 transizioni del pulsante sinistro con le coordinate logiche
originali e ne consegna una per frame. `ui_sdl2_input_begin` conserva questa
coda, mentre azzera tastiera e rotella. Chiamare `ui_sdl2_input_frame` una sola
volta per frame UI: una chiamata riuscita consuma una transizione.
Un tap richiede due frame; una raffica accumula latenza. Overflow del mouse
annulla i gesti tramite `focus_lost` e produce una diagnostica, senza riprodurre
un batch parziale. Non è garantito un ordinamento unico mouse/tastiera.
Gli host di esempio non devono più interrompere il polling al primo click. Preedit IME, touch e gamepad
non sono implementati. La demo scala il framebuffer logico alla superficie della
finestra: mantiene le coordinate, ma non offre rasterizzazione HiDPI dedicata.

## Verifiche e fonti

`make test-ui-sdl2` usa SDL dummy con rendering software reale. Verifica pixel,
clip annidati/vuoti, glifi UTF-8, immagine, resize, uguaglianza pixel tra immediato
e draw list, traduzione input, rifiuto di begin/end/resize fuori sequenza e recupero
dopo errori di disegno o clip sbilanciati. `make test-ui-sdl2-sanitize` aggiunge ASan/UBSan.
Questi target sono opzionali: `make test` non richiede SDL.

Riferimenti utilizzati: [superficie finestra SDL](https://wiki.libsdl.org/SDL2/SDL_GetWindowSurface),
[presentazione SDL](https://wiki.libsdl.org/SDL2/SDL_UpdateWindowSurface),
[rasterizzazione FreeType](https://freetype.org/freetype2/docs/tutorial/step1.html),
[kerning FreeType](https://freetype.org/freetype2/docs/tutorial/step2.html).
Evidenze locali: `evidence/ui-sdl2/`.
