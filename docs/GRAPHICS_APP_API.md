# Contratto pubblico GraphicsApp

`GraphicsApp::application` comprende APP, renderer Vulkan, input e adattatore UI.
Il consumatore include solo gli header installati e collega il target CMake;
non aggiunge `src/` o directory private del checkout agli include o ai sorgenti.
Un'installazione sceglie un solo host (`GraphicsApp_PLATFORM`): GLFW o SDL2.

L'SDK applicativo è ora **0.7.0**, con UIFramework **0.5.0**. Aggiunge
segnaposto, bottoni con icona, lista/albero e picker RGBA/HSV SDR. Il contratto
di callback e proprietà di APP rimane disponibile. In questa
fase pre-1.0, aggiunte all'API pubblica incrementano la minor dell'SDK interessato;
correzioni compatibili incrementano la patch. Il package CMake usa versioni
esatte e non promette compatibilità binaria fra versioni diverse.

## Ciclo di vita e proprietà

- `APP` appartiene al chiamante e parte da zero. Titolo, impostazioni video e
  `APP.resources` si configurano prima di `app_init`.
- `app_init` restituisce false sul fallimento e registra `APP.error`; non termina
  il processo. Dopo un fallimento si chiama comunque `app_shutdown`.
- Finestra, UI e contenitori GPU in `APP` sono posseduti dal framework. Non vanno
  distrutti separatamente o sostituiti mentre APP è attiva.
- `app_run` (oppure il loop legacy `app_loop`) ritorna per chiusura o errore; il chiamante esegue `app_shutdown`
  e legge infine `APP.error`, così considera anche eventuali errori di shutdown.
- `AppCallbacks.draw(APP *, void *)` compone la UI durante il frame aperto dal
  framework. Il chiamante passa il proprio stato a `app_run` tramite `user_data`.
- `app_loop` conserva il contratto precedente: richiede `app_draw(APP *)`
  nell'eseguibile. Chi usa solo `app_run` non deve definire questo simbolo.
- Il framework gestisce una sola applicazione attiva; lifecycle, UI e piattaforma
  si usano sul thread principale. Non è un'API multi-finestra o multithread.

Le stringhe di configurazione, incluso il titolo, sono prese in prestito e
restano valide fino allo shutdown. `APP.resources` conserva i puntatori e viene
riletta durante la ricreazione della swapchain. Non modificarla con APP attiva.
Gli stage impostati dal framework in `AppError.stage` sono testo statico: non vanno liberati; conservarli non trasferisce
proprietà di risorse GPU. Il layout pubblico di APP richiede di ricompilare
insieme libreria e consumatori dopo modifiche dell'API: non c'è ABI stabile.

## Callback e richiesta di chiusura

```c
typedef struct { int clicks; } MyState;
static void draw(APP *app, void *user_data) {
    MyState *state = user_data;
    /* Composizione UI, modificando state. */
    if (state->clicks >= 10) app_request_close(app);
}
/* Dopo app_init: */
/* MyState state = {0};
   const AppCallbacks callbacks = {.draw = draw};
   app_run(&app, &callbacks, &state); */
```

`draw` è obbligatoria; una tabella NULL o una callback draw NULL registra
`APP_ERROR_STATE` senza iniziare un frame. La tabella viene copiata all'ingresso:
il ciclo usa quella copia per tutta l'esecuzione. `user_data` può essere NULL;
se punta a uno stato, il chiamante lo mantiene valido fino al ritorno del ciclo.
Il framework non alloca né libera lo stato applicativo.

`close_requested(APP *, void *)` è opzionale: NULL accetta la chiusura.
Con una callback, true accetta e false consuma la richiesta continuando il ciclo.
Viene chiamata fuori dal frame UI, con il contesto UI dell'app selezionato.
Le richieste prima del loop, dagli eventi nativi, da `app_request_close` e dal
flag `platform_window_set_should_close` seguono la stessa politica. Anche il
limite di tempo diagnostico `APP_AUTOSTART_SECONDS` passa da questa politica.
Richieste ravvicinate prima della gestione condividono il flag della finestra.

Per un documento modificato, il controller apre il dialogo e restituisce false;
la callback draw compone il dialogo nei frame successivi. Dopo conferma o
salvataggio, l'app richiama `app_request_close` e la callback accetta. Gli errori
fatali terminano il ciclo direttamente, senza passare dal veto applicativo.

`app_request_close` non distrugge risorse ed è innocua con APP NULL/non
inizializzata. Il ciclo chiama la politica di chiusura solo fuori dal frame;
una richiesta durante draw viene gestita dopo `ui_end_frame`, prima del present.
`APP.running` è gestito dal framework: non modificarlo. Shutdown durante il
ciclo e loop annidati sulla stessa APP vengono rifiutati con `APP_ERROR_STATE`;
il chiamante effettua lo shutdown dopo il ritorno. Le callback non devono
liberare APP, alterare le risorse possedute o avviare altri loop/event pump.

## Header e dipendenze

| Area | Header principali | Uso |
| --- | --- | --- |
| Applicazione | `app/application.h`, `app/resources.h` | Lifecycle, errori e configurazione risorse |
| Rendering pubblico | `render/gfx.h`, `render/font.h`, `render/text_renderer.h`, `render/vsync_settings.h` | Operazioni grafiche senza handle Vulkan nativi |
| Piattaforma | `platform/window.h`, `platform/events.h`, `platform/key_codes.h` | Finestra opaca ed eventi indipendenti dall'host |
| File e percorsi | `platform/platform_fs.h`, `platform/paths.h` | I/O portabile e percorsi; la root storica globale non è la root di APP.resources |
| Input | `input/keyboard.h`, `input/mouse.h`, `input/scroll.h` | Raccolta degli eventi per loop personalizzati |
| UI | `ui/ui.h`, `ui/file_picker.h`, `ui/renderer.h`, `ui/draw_list.h` | API UI indipendente; contratti nel relativo SDK |
| Adattatore APP | `ui/app_adapter.h` | Collegamento fra APP e UI, specialmente per loop personalizzati |

Non sono pubblici `gfx_vulkan.h`, gli header `vulkan_*`, i bridge delle superfici
o gli header privati dei backend. Non vengono installati. Ogni header installato
viene compilato separatamente dal validatore, senza dipendere dall'ordine degli
include. Le dipendenze native vengono risolte dall'SDK, non dal codice dell'editor.

Le callback piattaforma sono slot condivisi: un handler aggiunto deve concatenare
quello restituito dal setter e ripristinarlo prima dello shutdown. Il puntatore
utente della finestra creata da APP è riservato al framework. Per la chiusura
l'editor usa `AppCallbacks.close_requested`: non sostituisce gli handler piattaforma.

## Risorse del consumatore CMake

```cmake
find_package(GraphicsApp 0.7.0 EXACT REQUIRED COMPONENTS application)
add_executable(my_app main.c)
target_link_libraries(my_app PRIVATE GraphicsApp::application)
graphics_app_deploy_resources(my_app)
```

`graphics_app_deploy_resources` copia le risorse dell'SDK in `resources/` accanto
all'eseguibile durante ogni build, senza richiedere un nuovo link. Si può chiamare
più volte per lo stesso target: la prima chiamata crea la regola. Richiede un
target eseguibile locale e un SDK completo disponibile. Gestisce anche directory
di output per configurazione tramite `$<TARGET_FILE_DIR:...>`.

Il codice applicativo imposta `app.resources.root_directory = "resources"`.
La risoluzione parte dalla directory dell'eseguibile e non dalla cwd. Il helper
non modifica APP né incorpora il percorso di installazione nel programma.
L'editor conserva la convenzione Make quando costruito con Make; nel progetto
CMake esterno configura la root distribuita. `--resources` permette un override
esplicito. `--root` riguarda invece i documenti e il contenimento del picker.

Il bundle comprende eseguibile e risorse; driver Vulkan e libreria dell'host
rimangono dipendenze di sistema. Copiare le risorse non significa distribuire
automaticamente driver o librerie native.

L'editor espone diagnostica opzionale con `UI_INPUT_DEBUG`: `[EDITOR NAV]`
riporta l'ID restituito da `ui_keyboard_focus_id()`. È distinto dal focus del
campo di testo riportato dal log UI e consente al validatore di raggiungere
azioni e voci del picker senza dipendere dal numero di pressioni di Tab.
La diagnostica non è attiva durante l'uso normale.
