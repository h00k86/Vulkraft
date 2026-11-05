# Piattaforma indipendente da GLFW — 27 ottobre 2026

Applicazione, UI, input e renderer usano contratti del framework. GLFW rimane
il backend grafico predefinito, sostituibile in compilazione; non sono stati
introdotti caricamento dinamico di plugin o un secondo renderer.

## Confini

- `include/platform/window.h`: `PlatformWindow` opaco, configurazione della
  finestra, polling/attesa, tempo, dimensioni logiche/framebuffer, posizione
  puntatore, stato tasti/pulsanti e callback.
- `include/platform/key_codes.h`: identificatori propri per 120 tasti,
  modificatori, azioni e pulsanti. I valori numerici non sono codici GLFW.
- `include/platform/events.h`: eventi tasto, carattere Unicode, focus,
  puntatore, pulsante, rotella, dimensioni logiche, framebuffer, iconificazione
  e richiesta di chiusura. `platform_set_event_callback` osserva gli eventi
  in ordine, senza consumarli o sostituire i gestori specifici.
- `src/platform/window.c`: adattatore fra impostazioni di APP e piattaforma;
  collega il resize del framebuffer al flag applicativo.
- `src/platform/backends/glfw.c`: implementazione GLFW, handle nativo privato,
  traduzione esplicita di tasti/modificatori, callback native e cache dimensioni.
- `include/render/platform_surface.h`: ponte specifico del renderer per
  estensioni d'istanza e creazione superficie Vulkan. Implementazioni in
  `src/render/backends/`; nessun tipo Vulkan entra nella UI o nella piattaforma pubblica.

`make check` rifiuta dipendenze GLFW fuori dagli adattatori e mantiene il
controllo dei confini Vulkan. Il probe X11 è un test d'integrazione esplicitamente
nativo e accede all'handle tramite un header privato, non tramite APP.

## Migrazione dell'applicazione

`APP.window` è ora `PlatformWindow *`. Non passarlo a funzioni GLFW e non
convertirlo con cast. Usare, per esempio:

```c
#include "platform/window.h"

platform_window_set_should_close(app->window, true);
int width, height;
platform_framebuffer_size(app->window, &width, &height);
```

Il normale `app_loop` gestisce già input e frame. Un loop personalizzato usa
`platform_poll_events` / `platform_wait_events` e gli stessi gestori input/UI.
Per il testo usare gli eventi Unicode, non ricostruire caratteri dai tasti.
Gli scancode sono valori nativi opachi e non identificatori portabili.

I setter delle callback specifiche restituiscono il vecchio gestore: un wrapper
osservatore deve concatenarlo. Le callback native appartengono al backend.
`create_window(APP*)` riserva il puntatore utente della finestra ad APP:
non sostituirlo. Le finestre create direttamente con `platform_window_create`
lasciano invece questo puntatore al chiamante.
Tutto avviene sul thread principale; rinviare la distruzione della finestra a
quando il polling è terminato. La coda dell'editor conserva ordine, repeat e
modificatori, scarta batch in overflow e gestisce la perdita di focus come prima.
Anche la rotella ripristina il vecchio gestore e azzera l'accumulo allo shutdown.

Lo stato dei widget è ora separato tramite [UIContext](UI_CONTEXT.md).
Input di piattaforma e renderer restano condivisi: il contratto applicativo
supportato resta una finestra attiva per volta.
Il mouse della UI continua a usare lo stato per frame; gli eventi completi sono
anche osservabili tramite la nuova callback generale.

## Build e backend di test

```sh
make PLATFORM=glfw BUILD=debug
make PLATFORM=glfw BUILD=release
make test
make test-sanitize
make test-platform
make PLATFORM=headless
```

Oggetti e librerie sono separati per backend. Le build GLFW conservano i nomi
abituali; quella headless produce `build/app-debug-headless.out` e
`build/libgraphics_app_debug_headless.a`. `build/app.out` punta all'ultima build
selezionata con il target predefinito.

`test-platform` collega UI, input e piattaforma reali con il backend headless,
senza GLFW, Vulkan o server grafico. La coda di test contiene fino a 2048 eventi;
`platform_headless_post` restituisce false se è piena o se l'evento non è valido.
Polling e wait consegnano un batch; gli eventi aggiunti dalle callback aspettano
il poll successivo. Il tempo avanza soltanto con `platform_headless_advance`.

Il backend headless **non crea una superficie Vulkan**: l'esempio grafico completo
compila senza GLFW ma termina con errore d'inizializzazione previsto allo stage
`instance`. Non è un renderer offscreen. Per una seconda piattaforma grafica
occorre implementare il contratto finestra/eventi e il ponte Vulkan.

## Verifica

- Build GLFW debug/release e build completa headless.
- 18 eseguibili in `make test` con backend predefinito; 15 in ASan/UBSan.
- Backend headless: tre cicli di creazione/distruzione, eventi ordinati,
  Unicode, repeat/modificatori, focus, mouse, rotella, resize HiDPI e chiusura.
- Mapping GLFW: round-trip di tutti i 120 tasti, codici sconosciuti e modificatori.
- `ldd`: nessuna dipendenza GLFW nell'eseguibile headless o nel test piattaforma.
- Runtime Intel UHD 620: Validation Layers e sync validation senza diagnostiche,
  tre resize, VSync off/on, UTF-8 visibile, chiusura regolare dello starter.
- Probe GLFW/X11 nascosto: press/release in frame distinti e nello stesso frame,
  attraverso il nuovo adattatore; nessuna automazione del mouse.

Evidenze in [evidence/platform-refactor](../evidence/platform-refactor/).
Non verificati altri driver/OS, un secondo backend grafico, perdita reale della
GPU o LeakSanitizer. Per i test aggiunti sul ciclo di vita e sul reset dei contesti UI vedere
[UIContext](UI_CONTEXT.md).
