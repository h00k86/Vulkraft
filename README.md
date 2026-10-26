# Graphics App Base

Base per applicazioni grafiche in C, estratta dal motore ENGINE: finestra GLFW,
renderer Vulkan, testo MSDF, widget e layout. Il repository e' autonomo: non
richiede il checkout del gioco e non carica livelli, personaggi o asset di gioco.

## Avvio

Servono un compilatore C11, Make, `ar`, GLFW 3, Vulkan e `glslc`.
Su Linux il collegamento usa `-lglfw -lvulkan -lm`.

```sh
make -j4
./build/app.out
make test
make BUILD=release -j4
```

Le build sono separate: `build/app-debug.out`, `build/app-release.out` e le
rispettive librerie `build/libgraphics_app_debug.a` e
`build/libgraphics_app_release.a`. `build/app.out` punta all'ultima build scelta.
Il programma trova shader e font rispetto all'eseguibile; si puo' avviare anche
da una directory diversa dal repository.

L'esempio mostra un campo di testo, pulsanti, contatore, tema chiaro/scuro,
checkbox, campo numerico e contenuto scorrevole. Il canvas occupa tutta la
finestra: coordinate logiche GLFW, framebuffer Vulkan alla risoluzione reale.

## Da dove iniziare una nuova applicazione

- `examples/main.c`: entrata del programma; imposta `app->title` e
  `app->settings.video` prima di `app_init()` per personalizzare la finestra.
- `examples/starter.c`: composizione dell'interfaccia dentro `app_draw()`.
  E' il punto da sostituire con la UI e lo stato della propria applicazione.
- `include/ui/ui.h`: API dei widget, layout, menu, dialoghi, picker e scroll.
- `include/render/text_renderer.h`: testo, rettangoli, linee, cerchi e immagini.
- `include/render/texture_manager.h`: caricamento e cache delle immagini;
  passa lo slot restituito a `draw_textured_quad()`.
- `include/platform/platform_fs.h`: file, directory e percorsi portabili.

`app_loop()` gestisce eventi, ridimensionamento, input della UI, inizio/fine
frame e presentazione. `app_draw()` e' fornita dall'applicazione e usa le API
esistenti. Non serve includere Vulkan nel codice dell'applicazione.
Le librerie statiche permettono di collegare una nuova composizione senza
portarsi dietro un altro eseguibile o il main della demo.

Il renderer conserva 16 slot texture: 0 per il font, 1-14 per immagini e icone,
15 per l'anteprima transiente del file picker. Lo shader usa gli indici costanti
in uno switch, cosi' questo percorso UI non richiede descriptor indexing.
Le immagini hanno la stessa convenzione di campionamento dell'engine; i colori
UI esadecimali passano da `ui_color_srgb()`.

## Architettura

| Strato | Responsabilita' | Dipendenze |
| --- | --- | --- |
| `examples/` | Stato e composizione della propria applicazione | API pubbliche |
| `src/app/` | Ciclo di vita e coordinamento del frame | UI, input, piattaforma, `gfx.h` |
| `src/ui/` | Widget, layout, focus, popup, dialoghi | Disegno pubblico e input GLFW |
| `src/input/` | Coordinate del puntatore e rotella | GLFW e contratto applicazione |
| `src/platform/` | Finestra, filesystem, risoluzione risorse | GLFW e API del sistema operativo |
| `src/render/` | Buffer, texture, shader, swapchain, submit e sincronizzazione | Vulkan privato |
| `src/math/` | Matrici | Libreria C |

`include/render/gfx.h` espone contratti indipendenti da Vulkan. Gli handle veri
vivono in `include/render/gfx_vulkan.h`, che resta privato al backend.
`APP` contiene puntatori opachi, font, configurazione della finestra e tempo:
nessuno stato di gioco. UI e applicazione non accedono alle strutture Vulkan.

`make check` verifica questi confini e l'assenza di dipendenze verso i moduli
rimossi. La UI legge GLFW come nel codice originale; non e' stata introdotta
una nuova astrazione degli eventi o un secondo backend.

## Cosa e' stato estratto

Sono riutilizzate le funzioni esistenti. I collegamenti a scene, livelli,
combattimento, audio, particelle, mesh, minimappa, post processing ed editor del
gioco sono stati rimossi. Le funzioni del menu prima abilitate dall'editor
sono ora disponibili come UI generica.

Le sole rinomine di funzioni sono:

| Originale | Nome nella base |
| --- | --- |
| `game_init` | `app_init` |
| `game_shutdown` | `app_shutdown` |
| `game_loop` | `app_loop` |
| `game_draw_hud` | `app_draw` |
| `game_autostart_frame_hook` | `app_autostart_frame_hook` |

I corpi sono stati ridotti o adattati nella copia: non e' una copia byte per
byte. Non sono state aggiunte funzioni C o GLSL prive di una definizione
originaria. `docs/extraction-manifest.json` riporta file, nome originale,
linea e hash di ogni definizione di partenza. Lo snapshot sorgente include
anche le modifiche locali presenti nel motore al momento dell'estrazione.
`python3 tools/check_boundaries.py --extraction` verifica l'inventario iniziale;
in seguito, quando aggiungi le funzioni della tua applicazione, usa il normale
`make check` per mantenere i confini architetturali.

## Limiti ereditati e verifiche

Il campo di testo esistente e' a riga singola e usa il polling dei tasti:
non implementa un editor multilinea, undo/redo, IME o un input Unicode completo.
Questi comportamenti appartengono all'applicazione futura e non sono stati
aggiunti durante l'estrazione. Il loop resta quello continuo del motore,
con V-Sync e attesa degli eventi quando la finestra e' minimizzata.

Il filesystem conserva i rami Windows/POSIX gia' presenti; build e runtime
vengono verificati qui su Linux. Non e' stata effettuata una verifica su
Windows o macOS. I font e stb_image sono copiati dal progetto; le condizioni
presenti nei sorgenti di terze parti sono mantenute. Non viene assegnata una
nuova licenza al codice originale.

I log e le catture effettivamente eseguite sono in `evidence/` e il resoconto
in `docs/VALIDATION.md`.

Per una prova breve senza toccare le impostazioni dell'engine:

```sh
APP_AUTOSTART_SECONDS=8 ./build/app.out
```

La cattura opzionale usa `APP_FRAME_DUMP_DIR`, `APP_FRAME_DUMP_FPS` e
`APP_FRAME_DUMP_MAX` (directory da creare prima). Le preferenze del picker,
quando richieste dal chiamante, usano l'identita' separata `graphics-app-base`.
