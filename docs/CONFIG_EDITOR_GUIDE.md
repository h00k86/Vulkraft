# Creare un'applicazione partendo dall'editor

L'editor di configurazione è un esempio di riferimento per un'applicazione con
modello, proprietà modificabili, undo/redo, file e conferma di chiusura.
Questa guida usa l'host Vulkan di GraphicsApp 0.5.1, disponibile nel checkout
completo e nel relativo SDK. Il pacchetto UIFramework da solo comprende invece
il toolkit UI e l'esempio con host SDL software.

## Mappa del codice

Tutti i file applicativi sono in `examples/config_editor/`:

| File | Responsabilità | Da cambiare nella propria app |
| --- | --- | --- |
| `document.h/.c` | Dati, validazione, lettura e scrittura del formato | Strutture dati e formato del documento |
| `editor.h/.c` | Documento corrente/salvato, cronologia, azioni sospese e conflitti | Operazioni e regole applicative |
| `view.h/.c` | Widget, layout, dialoghi e picker | Composizione dell'interfaccia |
| `vulkan_main.c` | Stato dell'app, configurazione di APP e callback | Titolo, risorse, avvio e argomenti |
| `CMakeLists.txt` | Consumer dell'SDK installato e distribuzione delle risorse | Nome del target e sorgenti applicativi |

`document.c` e `editor.c` usano il filesystem portabile ma non conoscono widget,
finestra o Vulkan. La vista usa le API UI senza dipendere da APP. Il main collega
questa composizione al ciclo applicativo. Il framework non conosce `.cfgapp`.

## Stato, callback e proprietà

`EditorApplication`, nel main reale, contiene il controller e lo stato di
avvio/diagnostica. Il main ne possiede la memoria e lo passa a `app_run` come
`user_data`; draw e politica di chiusura ricevono lo stesso puntatore.
APP possiede finestra, UI e risorse grafiche. Lo shutdown libera tali risorse,
mentre lo stato applicativo resta del chiamante.

Questo bootstrap minimo usa la directory corrente per i documenti e le risorse
SDK distribuite accanto all'eseguibile. Può sostituire `vulkan_main.c` in una
copia dell'esempio; la CLI completa resta nel main originale.

```c
#include "view.h"
#include "app/application.h"
#include <stdio.h>

typedef struct { ConfigEditor editor; } MyApplication;

static bool close_requested(APP *app, void *user_data) {
    (void)app;
    MyApplication *state = user_data;
    config_editor_request(&state->editor, CONFIG_ACTION_CLOSE);
    return state->editor.close_ready;
}

static void draw(APP *app, void *user_data) {
    MyApplication *state = user_data;
    config_editor_draw(&state->editor, &app->renderer.font, VIRTUAL_W, VIRTUAL_H);
    if (state->editor.close_ready) app_request_close(app);
}

int main(void) {
    MyApplication state = {0};
    APP app = {0};
    const AppCallbacks callbacks = {.draw = draw, .close_requested = close_requested};
    if (!config_editor_init(&state.editor, ".")) {
        fprintf(stderr, "%s\n", state.editor.message);
        return 1;
    }
    app.title = "La mia applicazione";
    app.resources.root_directory = "resources";
    bool ready = app_init(&app);
    if (ready) app_run(&app, &callbacks, &state);
    app_shutdown(&app);
    if (!ready || app.error.code) {
        fprintf(stderr, "%s\n", app.error.stage ? app.error.stage : "Avvio fallito");
        return 1;
    }
    return 0;
}
```

La tabella callback è copiata da `app_run`. Stato e stringhe di configurazione
restano validi per il tempo richiesto dal contratto; in questo esempio vivono
fino al ritorno del main. Il framework supporta una sola APP attiva sul thread
principale.

## Chi apre il frame e chi raccoglie l'input

Con l'host GraphicsApp, `app_init` collega il renderer UI e i servizi input e
clipboard. `app_run` gestisce eventi, canvas, frame UI, popup e presentazione.
La callback draw compone i widget dentro quel frame: non richiama
`ui_begin_frame`, `ui_end_frame`, polling eventi o `app_shutdown`.

Per un host già esistente, il punto riutilizzabile è `config_editor_draw`:
il chiamante seleziona il proprio `UIContext`, collega un `UIRenderer`, prepara
`UIFrameInput`, apre il frame e compone la vista. Dopo la vista disegna i popup e
chiude il frame. La modalità a input esplicito è mostrata in
`examples/ui_standalone.c`. Le callback di misura del testo e disegno devono
concordare su font, coordinate e scala; clipboard e anteprime sono servizi
configurati separatamente. I contratti completi sono negli header pubblici
`ui/renderer.h`, `ui/ui.h` e `ui/file_picker.h`.

## Modifiche, salvataggio e chiusura

Una modifica ai dati viene seguita da `config_editor_checkpoint`. Le digitazioni
usano gruppi di cronologia; le azioni discrete usano gruppo zero. La vista
costruisce gli ID dei campi con generazione del documento e identità del profilo:
conservare questa regola evita di riusare focus e cronologia di un altro documento.
Lo stato dirty deriva dal confronto dei dati con `saved`, non dal solo cursore undo.

Nuovo, Apri e Chiudi passano da `config_editor_request`. Se il documento è dirty,
il controller conserva l'azione in `pending` e apre la conferma. La vista presenta
Salva/Scarta/Annulla e inoltra la decisione al controller. Salva può aprire il picker;
solo un salvataggio riuscito riprende l'azione sospesa. Un errore conserva i dati
modificati e annulla l'azione sospesa.

La richiesta nativa di chiusura e `app_request_close` raggiungono
`close_requested` fuori dal frame UI. Restituire false rinvia la chiusura e permette
alla draw di presentare la conferma. Quando `close_ready` diventa true, la draw
richiede nuovamente la chiusura; il ciclo ritorna e il main esegue lo shutdown.
Gli errori fatali di APP fanno terminare il ciclo senza passare dal veto di chiusura.

Gli errori del documento sono in `ConfigEditor.message` e vengono mostrati nella
vista; quelli del framework sono in `APP.error`. Controllare questi ultimi dopo
lo shutdown, anche quando init fallisce. Il controllo dei conflitti sul file è
ottimistico: non sostituisce un lock o il recupero da crash.

## Costruire il proprio progetto con l'SDK

Dalla root del checkout completo, con le dipendenze Vulkan/GLFW e `glslc` installate:

```sh
cmake -S . -B build/guide-sdk -DGRAPHICS_APP_WITH_VULKAN=ON \
  -DGRAPHICS_APP_PLATFORM=glfw -DCMAKE_BUILD_TYPE=Debug \
  -DUI_BUILD_EXAMPLES=OFF -DGRAPHICS_APP_BUILD_EXAMPLE=OFF -DBUILD_TESTING=OFF
cmake --build build/guide-sdk -j4
cmake --install build/guide-sdk --prefix /tmp/vulkraft-guide-sdk

mkdir -p /tmp/vulkraft-editor-src /tmp/vulkraft-editor-documents
cp /tmp/vulkraft-guide-sdk/share/graphics-app/examples/config_editor/* /tmp/vulkraft-editor-src/
cmake -S /tmp/vulkraft-editor-src -B /tmp/vulkraft-editor-build \
  -DCMAKE_PREFIX_PATH=/tmp/vulkraft-guide-sdk -DCMAKE_BUILD_TYPE=Debug
cmake --build /tmp/vulkraft-editor-build -j4
/tmp/vulkraft-editor-build/config_editor --root /tmp/vulkraft-editor-documents
```

I comandi usano la CLI del main originale. Per SDL2/Vulkan scegliere `sdl2` nella
configurazione dell'SDK, usando un'altra directory di build/installazione.
L'editor esterno usa solo header e librerie installati; `graphics_app_deploy_resources`
copia font e shader nella directory `resources` del bundle.

Per iniziare una propria app, copiare i sorgenti applicativi dell'esempio e il suo
CMakeLists, poi adattare modello, controller, vista e target. Gli hook `--smoke`,
`config_editor_smoke_event/result` e il log `[EDITOR NAV]` servono alla validazione
dell'esempio; il bootstrap sopra mostra il percorso applicativo essenziale.

`--root` riguarda i documenti e deve indicare una directory esistente;
`APP.resources.root_directory` riguarda font e shader. La root risorse relativa
è risolta rispetto all'eseguibile, mentre un percorso CLI `--open` relativo parte
dalla directory di lavoro e deve comunque ricadere nella root documenti.

## Verificare le proprie modifiche

`make test-config-editor test-config-editor-sanitize` verifica modello, controller
e workflow UI dell'esempio, senza provarne l'aspetto grafico. Dopo aver modificato
il formato o le azioni, adattare queste prove alla propria applicazione.
`--smoke` del main originale prova la composizione su Vulkan senza input desktop;
focus e interazioni native richiedono le prove dedicate.

Formato, cronologia, comportamento dei conflitti e limiti del filesystem sono
approfonditi in [CONFIG_EDITOR.md](CONFIG_EDITOR.md).
