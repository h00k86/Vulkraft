# Vulkraft

Framework per applicazioni grafiche in C11, basato su Vulkan, con backend
piattaforma GLFW e SDL2, testo MSDF, widget e layout. Il repository contiene
sorgenti, risorse, esempi e strumenti necessari per compilare il framework.

Per integrare una nuova applicazione usare `AppCallbacks`, `app_run()` e
`user_data`: [contratto pubblico GraphicsApp 0.5.1](docs/GRAPHICS_APP_API.md).
I nomi dei target CMake e delle API rimangono `GraphicsApp` e `UIFramework`.

Build ed evidenze generate dalle verifiche restano locali; i comandi per
riprodurle sono descritti nella documentazione di validazione.

## Avvio

Servono un compilatore C11, Make, `ar`, GLFW 3, Vulkan e `glslc`.
Su Linux il collegamento usa `-lglfw -lvulkan -lm`.

```sh
make -j4
./build/app.out
make test
make BUILD=release -j4
```

Per la verifica completa del framework usare `make validate`; la variante
`make validate-native` aggiunge interazioni sul desktop X11/i3.
`make benchmark-ui` misura il costo CPU del core. Comandi, report e limiti in
[Validazione del framework](docs/PROJECT_VALIDATION.md).

Le build sono separate: `build/app-debug.out`, `build/app-release.out` e le
rispettive librerie `build/libgraphics_app_debug.a` e
`build/libgraphics_app_release.a`. `build/app.out` punta all'ultima build scelta.
Il programma trova shader e font rispetto all'eseguibile; si puo' avviare anche
da una directory diversa dal repository.

L'esempio mostra un campo di testo, pulsanti, contatore, tema chiaro/scuro,
checkbox, campo numerico e contenuto scorrevole. Il canvas occupa tutta la
finestra: coordinate logiche della piattaforma, framebuffer Vulkan alla risoluzione reale.

## UI senza finestra o GPU

`make test-ui-standalone` compila ed esegue un consumatore delle sole API pubbliche,
con input sintetico e backend simulato, senza GLFW/Vulkan. Produce anche
`build/libui_debug.a`. Per il codice, il percorso release e i limiti:
[Esempio standalone](docs/UI_STANDALONE.md). Il [file picker](docs/UI_FILE_PICKER.md)
è un modulo opzionale separato: `make ui-picker` produce la libreria aggiuntiva.

## Da dove iniziare una nuova applicazione

- `examples/main.c`: entrata del programma; imposta `app->title` e
  `app->settings.video` prima di `app_init()` per personalizzare la finestra.
  `app_init()` restituisce `bool`: controllarne l’esito prima del loop; in caso
  di errore `app->error` espone codice e fase. `app_shutdown()` accetta anche
  un avvio parziale e chiamate ripetute.
- `examples/starter.c`: composizione dell'interfaccia dentro `app_draw()`.
  E' il punto da sostituire con la UI e lo stato della propria applicazione.
- `include/ui/ui.h`: API dei widget, layout, menu, dialoghi, picker e scroll.
- `include/render/text_renderer.h`: testo, rettangoli, linee, cerchi e immagini.
- `include/render/texture_manager.h`: caricamento e cache delle immagini;
  passa lo slot restituito a `draw_textured_quad()`.
- `include/platform/window.h`: finestra opaca, eventi, tempo e input portabili.
- `include/platform/platform_fs.h`: file, directory e percorsi portabili.

`app_loop()` gestisce eventi, ridimensionamento, input della UI, inizio/fine
frame e presentazione. `app_draw()` e' fornita dall'applicazione e usa le API
esistenti. Non serve includere Vulkan nel codice dell'applicazione.
Ogni APP possiede ora un `UIContext`: focus, popup, tema e stato dei widget
sono separati per contesto. Il loop lo seleziona automaticamente; per l'uso
manuale e i limiti del renderer condiviso vedere [Contesti UI](docs/UI_CONTEXT.md).
La UI accetta anche [input di frame espliciti](docs/UI_FRAME_INPUT.md), senza
leggere APP o la finestra per acquisire eventi e dimensioni del canvas.
Il [backend di rendering UI](docs/UI_RENDERER.md) è sostituibile per contesto;
APP collega automaticamente l’adattatore al renderer Vulkan esistente.
Una [draw list opzionale](docs/UI_DRAW_LIST.md) permette di registrare i comandi
e riprodurli successivamente sul backend scelto.
Le librerie statiche permettono di collegare una nuova composizione senza
portarsi dietro un altro eseguibile o il main della demo.

Il renderer conserva 16 slot texture: 0 per il font, 1-14 per immagini e icone,
15 per l'anteprima transiente del file picker. Lo shader usa gli indici costanti
in uno switch, cosi' questo percorso UI non richiede descriptor indexing.
I colori UI esadecimali passano da `ui_color_srgb()` per la conversione
nello spazio colore usato dal renderer.

## Architettura

| Strato | Responsabilita' | Dipendenze |
| --- | --- | --- |
| `examples/` | Stato e composizione della propria applicazione | API pubbliche |
| `src/app/` | Ciclo di vita e coordinamento del frame | UI, input, piattaforma, `gfx.h` |
| `src/ui/` | Widget, layout, focus, popup, dialoghi | Disegno pubblico e input del framework |
| `src/input/` | Coordinate del puntatore e rotella | API della piattaforma e contratto applicazione |
| `src/platform/` | Contratti finestra/eventi, filesystem e risorse | Backend selezionato e API del sistema operativo |
| `src/render/` | Buffer, texture, shader, swapchain, submit e sincronizzazione | Vulkan privato |
| `src/math/` | Matrici | Libreria C |

`include/render/gfx.h` espone contratti indipendenti da Vulkan. Gli handle veri
vivono in `include/render/gfx_vulkan.h`, che resta privato al backend.
`APP` contiene puntatori opachi, font, configurazione della finestra e tempo.
UI e applicazione non accedono alle strutture Vulkan.

`make check` verifica questi confini e le dipendenze tra i moduli. GLFW è confinato negli adattatori finestra/superficie; app, UI e input
usano API del framework. Un backend headless verifica il contratto senza GLFW.
Build, migrazione e limiti: [PLATFORM_ABSTRACTION.md](docs/PLATFORM_ABSTRACTION.md).

## Limiti e verifiche

L'editing di testo è a riga singola, supporta UTF-8 e undo/redo; IME,
segmentazione per grafemi ed editor multilinea non sono implementati.
Il ciclo standard è continuo, con V-Sync e attesa eventi a finestra minimizzata.

Il filesystem comprende implementazioni Windows/POSIX. Le verifiche descritte
sono eseguite su Linux; Windows e macOS richiedono validazione dedicata.
Le licenze delle dipendenze sono conservate nei relativi sorgenti e asset.

Comandi di validazione e limiti delle prove sono descritti in
[PROJECT_VALIDATION.md](docs/PROJECT_VALIDATION.md). Log e catture vengono
generati localmente in `evidence/`.

Per una prova breve dell'applicazione:

```sh
APP_AUTOSTART_SECONDS=8 ./build/app.out
```

La cattura opzionale usa `APP_FRAME_DUMP_DIR`, `APP_FRAME_DUMP_FPS` e
`APP_FRAME_DUMP_MAX` (directory da creare prima). Le preferenze del picker,
quando richieste dal chiamante, usano l'identita' separata `graphics-app-base`.

Il ciclo standard registra le callback della tastiera e conserva gli eventi
tra frame. Per loop personalizzati vedere [INPUT_EVENT_FIX.md](docs/INPUT_EVENT_FIX.md).
Editor e renderer supportano UTF-8, con atlante latino esteso e fallback per
i glifi mancanti: [UTF8_SUPPORT.md](docs/UTF8_SUPPORT.md).

Gli errori fatali di rendering fermano il loop e sono disponibili in `APP.error`;
il chiamante esegue `app_shutdown`. Contratti e test in
[RENDER_ERROR_FIXES.md](docs/RENDER_ERROR_FIXES.md).

Migrazione API widget senza APP*: [UI_WIDGET_API.md](docs/UI_WIDGET_API.md).

Font opachi e callback delle metriche: [UI_FONT_BACKEND.md](docs/UI_FONT_BACKEND.md).

Decorazioni facoltative del backend: [UI_OPTIONAL_SKIN.md](docs/UI_OPTIONAL_SKIN.md).

Servizi anteprima facoltativi: [UI_OPTIONAL_PREVIEWS.md](docs/UI_OPTIONAL_PREVIEWS.md).

Gestione del clipping dei contenitori: [UI_SCOPE_CLIPS.md](docs/UI_SCOPE_CLIPS.md).

Errori e recupero della draw list: [UI_DRAW_LIST_ERRORS.md](docs/UI_DRAW_LIST_ERRORS.md).

## Backend SDL, scena comune e package

`make ui-reference-sdl2` costruisce la demo software SDL senza GLFW/Vulkan.
`make ui-reference-vulkan` usa la stessa scena con il renderer originale.
`make test-ui-sdl2` verifica pixel e input senza finestra; dipendenze opzionali.

- [Backend e demo SDL](docs/UI_SDL2.md)
- [API 0.3.1](docs/UI_API.md)
- [Navigazione da tastiera](docs/UI_KEYBOARD_NAVIGATION.md)
- [Editing, clipboard e undo/redo](docs/UI_TEXT_EDITING.md)
- [Package CMake e distribuzione sorgente](docs/UI_PACKAGE.md)

### Editor di configurazione

Un'applicazione completa con profili, proprietà, picker e gestione delle modifiche non salvate:

```sh
make config-editor-sdl2
./build/config-editor-sdl2-debug.out
# Stessa UI su Vulkan
make config-editor-vulkan
./build/config-editor-vulkan-debug.out
```

[Guida per creare una propria applicazione](docs/CONFIG_EDITOR_GUIDE.md) ·
[Formato e verifiche](docs/CONFIG_EDITOR.md).

## Host SDL2 con renderer Vulkan

Il checkout completo supporta anche `make PLATFORM=sdl2` e
`make PLATFORM=sdl2 config-editor-vulkan ui-reference-vulkan`, mantenendo le
stesse applicazioni e il renderer Vulkan. Oggetti e binari restano distinti
per backend. Comandi, dipendenze e verifiche in
[PLATFORM_SDL2_VULKAN.md](docs/PLATFORM_SDL2_VULKAN.md).

## SDK applicativo per progetti esterni

Il checkout completo può esportare `GraphicsApp::application` tramite CMake,
con host GLFW oppure SDL2 e risorse configurabili in `APP.resources`.
L'esempio [external_app](examples/external_app/CMakeLists.txt) usa soltanto
l'SDK installato e copia font/shader accanto all'eseguibile.
Istruzioni in [GRAPHICS_APP_SDK.md](docs/GRAPHICS_APP_SDK.md).

Anche l'[editor di configurazione](examples/config_editor/CMakeLists.txt) può
essere compilato fuori dal checkout con `find_package(GraphicsApp 0.5.1 EXACT)`.
La funzione `graphics_app_deploy_resources(target)` distribuisce font e shader
accanto all'eseguibile. Contratti pubblici e proprietà delle risorse in
[GRAPHICS_APP_API.md](docs/GRAPHICS_APP_API.md).

La [distribuzione sorgenti GraphicsApp](docs/GRAPHICS_APP_DISTRIBUTION.md)
include entrambi gli host, risorse, esempi e test, con manifest e checksum.
`make app-package` prepara l'archivio; `make validate` verifica anche gli SDK e
gli editor esterni ricostruiti da quell'archivio.

La revisione dei residui e delle API senza chiamanti interni è documentata in
[PORTING_CLEANUP.md](docs/PORTING_CLEANUP.md). Per selezionare la GPU si può usare
`APP_GPU=integrated`, `APP_GPU=discrete` oppure una parte del nome del dispositivo.

Le regole di Esc, focus e conservazione delle etichette della UI 0.3.1 sono
riassunte in [UI_INTEGRATION_BEHAVIOR.md](docs/UI_INTEGRATION_BEHAVIOR.md).
