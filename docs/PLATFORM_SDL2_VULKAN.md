# Host SDL2 per l'applicazione Vulkan

Il checkout completo può usare GLFW oppure SDL2 mantenendo lo stesso renderer
Vulkan, gli stessi collector e gli stessi sorgenti delle applicazioni.
La scelta avviene al collegamento tramite `PLATFORM`; i tipi SDL rimangono
negli adattatori privati. La prima versione dell'host SDL2 ammette una sola
finestra attiva, coerentemente con l'applicazione e i collector correnti.

## Compilazione e avvio

```sh
make PLATFORM=sdl2 -j4
make PLATFORM=sdl2 -j4 config-editor-vulkan ui-reference-vulkan
./build/config-editor-vulkan-debug-sdl2.out
```

Per GLFW:

```sh
make PLATFORM=glfw -j4 config-editor-vulkan ui-reference-vulkan
./build/config-editor-vulkan-debug.out
```

`BUILD=release` genera i corrispondenti eseguibili `-release`.
I binari e gli oggetti dei due backend occupano percorsi distinti:

| Target / eseguibile debug | Finestra | Rendering |
| --- | --- | --- |
| `PLATFORM=glfw config-editor-vulkan` / `config-editor-vulkan-debug.out` | GLFW | Vulkan |
| `PLATFORM=sdl2 config-editor-vulkan` / `config-editor-vulkan-debug-sdl2.out` | SDL2 | Vulkan |
| `config-editor-sdl2` / `config-editor-sdl2-debug.out` | SDL2 | Software SDL2 |

`make PLATFORM=sdl2 run` avvia l'esempio base. Il percorso SDL2 collega SDL2
come libreria finestra; il confronto dei collegamenti verifica l'assenza di
GLFW nei binari SDL2 e di SDL nei binari GLFW.

L'host richiede SDL2 >= 2.26, pkg-config e le dipendenze Vulkan già usate dal
checkout. Il requisito 2.26 deriva da
[`SDL_GetWindowSizeInPixels`](https://wiki.libsdl.org/SDL2/SDL_GetWindowSizeInPixels),
che distingue i pixel del framebuffer dalle coordinate logiche dell'input.
La distribuzione UI standalone mantiene componenti e requisiti propri: il suo
backend software SDL2 resta distinto dall'host APP/Vulkan costruito con Make.
Questa modifica non cambia le API UI o l'archivio SDK di riferimento 0.1.0.

## Proprietà e raccolta degli eventi

`src/platform/backends/sdl2.c` implementa `platform/window.h`:

- Inizializzazione video con `SDL_InitSubSystem`, bilanciata da
  `SDL_QuitSubSystem`. Un proprietario video esterno mantiene il proprio
  riferimento; il backend ripristina lo stato del text input che ha attivato.
- Finestra ridimensionabile con flag Vulkan/HiDPI; configurazioni windowed,
  fullscreen e borderless. Le prove native correnti usano windowed.
- Callback specifiche e osservatore generale concatenabili. La chiusura
  imposta prima il flag, permettendo all'osservatore di annullarla.
- Tasti fisici da scancode SDL; caratteri esclusivamente dai commit
  `SDL_TEXTINPUT`, decodificati in scalari UTF-8. F25 non ha equivalente SDL2;
  i tasti sconosciuti mantengono `PLATFORM_KEY_UNKNOWN`.
- Stati di tasti/pulsanti e coordinate aggiornati prima delle callback.
  I clic conservano le coordinate originali anche quando il polling include
  successivi movimenti o altri tap. Il collector esistente consegna una
  transizione per frame e annulla i gesti su perdita del focus/overflow.
- Resize logico e framebuffer distinti; framebuffer a zero durante la
  minimizzazione. `platform_wait_events()` consegna anche il primo evento
  ricevuto e poi scarica quelli già disponibili.
- Clipboard posseduta dalla finestra: lettura presa in prestito fino alla
  successiva operazione clipboard o alla distruzione.

L'host possiede la coda SDL del processo. Gli eventi di finestre diverse
vengono ignorati; non è previsto un secondo event loop SDL concorrente.
Rimangono i vincoli di thread principale e assenza di distruzione della
finestra durante il dispatch. IME/preedit e grafemi non ampliano i contratti UI.

## Collegamento al renderer

`src/render/backends/sdl2_surface.c` implementa il ponte già esistente:

- Estensioni ottenute con
  [`SDL_Vulkan_GetInstanceExtensions`](https://wiki.libsdl.org/SDL2/SDL_Vulkan_GetInstanceExtensions).
  L'array di puntatori è posseduto dalla finestra e cresce su richiesta;
  gli errori restituiscono NULL e conteggio zero, conservando la vecchia riserva.
- Superficie creata con
  [`SDL_Vulkan_CreateSurface`](https://wiki.libsdl.org/SDL2/SDL_Vulkan_CreateSurface).
  In caso di fallimento l'handle di uscita resta nullo. Il renderer distrugge
  superficie e istanza prima della finestra, come nel percorso GLFW.

Nessun tipo Vulkan entra nel modulo della piattaforma; nessun tipo SDL entra
negli header pubblici della piattaforma o nel core UI. Il controllo
architetturale verifica anche i confini delle dipendenze SDL.

## Ripetere le verifiche

```sh
make test-platform-sdl2 test-platform-sdl2-sanitize
python3 tools/validate_platform_backends.py --output-dir evidence/platform-sdl2/build-tests
python3 tools/validate_platform_backends.py --runtime --output-dir evidence/platform-sdl2/runtime
```

La matrice costruisce debug/release di app, editor e scene su GLFW/SDL2,
controlla le dipendenze dinamiche e ricompila anche il backend headless.
Con `--runtime` apre finestre Vulkan e verifica editor, basic, navigazione,
editing, layout e colori su entrambi gli host, con validation layer attivo.
Gli eventi delle scene sono interni all'app e non vengono inviati al desktop.

Per la sequenza nativa, lasciando liberi mouse e tastiera:

```sh
python3 tools/config_editor_native_validation.py --backend vulkan --vulkan-platform sdl2 --quick-clicks --document-history --focus-cycle --output-dir evidence/platform-sdl2/native-sdl2
python3 tools/config_editor_native_validation.py --backend vulkan --vulkan-platform glfw --quick-clicks --document-history --focus-cycle --output-dir evidence/platform-sdl2/native-glfw
```

Lo script verifica il focus prima/dopo gli input e si interrompe se passa a
una finestra esterna. I report identificano il backend finestra del Vulkan host.

La suite senza GPU usa una finestra SDL dummy, eventi riprodotti all'ingresso
di Poll/WaitEvent e wrapper per creazione Vulkan, scala dei pixel e superficie.
Verifica traduzione, collector, lifecycle e fallimenti; non certifica il driver.
Il replay evita il limite dell'iniezione artificiale di `SDL_TEXTINPUT` nella
[versione SDL2-compat 2.32.74 installata](https://github.com/libsdl-org/sdl2-compat/blob/release-2.32.74/src/sdl2_compat.c).
La ricezione reale del testo è verificata separatamente dalla prova nativa.

Risultati e limiti sono in `evidence/platform-sdl2/`. Restano distinti:
sessione manuale, monitor HiDPI reale, fullscreen nativo, Wayland, altri
sistemi/GPU, prestazioni GPU e LeakSanitizer.
