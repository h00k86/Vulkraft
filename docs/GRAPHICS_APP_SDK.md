# SDK applicativo Vulkan

Il checkout completo può installare due pacchetti distinti: `UIFramework` per
la UI indipendente, `GraphicsApp` per ciclo applicativo, input e renderer Vulkan.
Il pacchetto sorgenti UI continua a contenere soltanto la UI. Per costruire
`GraphicsApp` serve il checkout completo oppure la
[distribuzione sorgenti GraphicsApp](GRAPHICS_APP_DISTRIBUTION.md).

## Compilazione e installazione

Prerequisiti: C11, CMake >= 3.20, Vulkan >= 1.3 con `glslc`, GLFW >= 3.3 oppure
SDL2 >= 2.26.0 (quest'ultimo scoperto tramite pkg-config). Il backend Vulkan
SDL2 non richiede SDL2_image o FreeType; sono dipendenze del backend UI software.

```sh
cmake -S . -B build/sdk-glfw -DCMAKE_BUILD_TYPE=Debug \
  -DGRAPHICS_APP_WITH_VULKAN=ON -DGRAPHICS_APP_PLATFORM=glfw
cmake --build build/sdk-glfw -j4
ctest --test-dir build/sdk-glfw --output-on-failure
cmake --install build/sdk-glfw --prefix /tmp/graphics-sdk-glfw
```

Per SDL2 usare una build e un prefisso distinti con `GRAPHICS_APP_PLATFORM=sdl2`.
Ogni installazione contiene un solo host: non si collegano due host nello
stesso processo. Le build Make (`PLATFORM=glfw|sdl2`) restano disponibili.
`UI_WITH_SDL2` seleziona invece il renderer UI software ed è indipendente.

## Consumatore esterno

```cmake
find_package(GraphicsApp 0.3.1 EXACT REQUIRED)
target_link_libraries(my_app PRIVATE GraphicsApp::application)
```

L'eseguibile configura `AppCallbacks` e passa il proprio stato tramite
`app_run(&app, &callbacks, &state)`. Il ciclo è `APP app = {0}` → `app_init` →
`app_run` → `app_shutdown`, anche dopo un init
fallito. Resta supportata una sola applicazione attiva per processo.
`GraphicsApp_PLATFORM` indica l'host dell'SDK trovato.
`GraphicsApp_RESOURCE_DIR` è il percorso delle risorse dell'installazione
corrente e viene ricalcolato dopo uno spostamento dell'SDK.

L'esempio `examples/external_app` è un progetto CMake autonomo; l'installazione
ne contiene una copia in `share/graphics-app/examples/external_app`:

```sh
cmake -S examples/external_app -B /tmp/external-build \
  -DCMAKE_PREFIX_PATH=/tmp/graphics-sdk-glfw
cmake --build /tmp/external-build -j4
cd /tmp
./external-build/external_app --smoke
```

CMake copia le risorse dell'SDK accanto all'eseguibile in `resources/`.
La regola è fornita da `graphics_app_deploy_resources(target)` e viene eseguita
anche se il programma non richiede un nuovo link.
Si può spostare insieme l'eseguibile e questa cartella: non incorpora percorsi
assoluti dell'SDK o del repository. Il driver Vulkan e l'host selezionato devono
essere disponibili sul sistema di destinazione; non è un bundle di tali librerie.

## Editor di configurazione esterno

Anche `examples/config_editor` è un progetto CMake autonomo. L'installazione ne
contiene tutti i sorgenti applicativi in `share/graphics-app/examples/config_editor`;
il modello, il controller e la vista appartengono all'applicazione, mentre
finestra, input, widget e rendering provengono esclusivamente dall'SDK:

```sh
cmake -S examples/config_editor -B /tmp/editor-build \
  -DCMAKE_PREFIX_PATH=/tmp/graphics-sdk-glfw -DCMAKE_BUILD_TYPE=Debug
cmake --build /tmp/editor-build -j4
mkdir -p /tmp/editor-documents
cd /tmp
./editor-build/config_editor --root /tmp/editor-documents
```

Il consumer non compila file `src/` del framework e non include directory private.
Si può copiare fuori dal checkout la sola cartella dell'esempio. `--open file`
apre un documento; `--resources dir` cambia la posizione delle risorse grafiche.
La root dei documenti e quella delle risorse sono indipendenti. `--smoke` esegue
la prova UI automatica senza generare input desktop. I target Make dell'editor
continuano a funzionare con la convenzione dei percorsi precedente.

Proprietà delle risorse, lifecycle, callback e superficie pubblica sono descritti
in [GRAPHICS_APP_API.md](GRAPHICS_APP_API.md).

## Risorse per applicazione

Configurare `app.resources` prima di `app_init`:

```c
app.resources.root_directory = "resources";
/* Override opzionali, relativi alla root oppure assoluti: */
app.resources.font_metadata = "assets/fonts/font_utf8.json";
app.resources.font_atlas = "assets/fonts/font_utf8.png";
app.resources.vertex_shader = "shaders/vert.spv";
app.resources.fragment_shader = "shaders/frag.spv";
app.resources.atlas_width = 1024;
app.resources.atlas_height = 1024;
```

Una root relativa parte dalla directory dell'eseguibile, indipendentemente dalla
directory di lavoro. Una root assoluta usa la posizione indicata. Un filename
assoluto prevale sulla root. Campi NULL conservano i default; root NULL conserva
la convenzione storica della root del checkout (rimozione della cartella `build`
dalla directory dell'eseguibile). Dimensioni zero conservano il default 1024.
I filename vuoti sono errori; una root vuota è rifiutata per i filename relativi.
Le stringhe sono prese in prestito e devono
restare valide e immutate fino a `app_shutdown`: gli shader vengono riletti
anche durante la ricreazione della swapchain.

`app_resource_path` risolve senza aprire i file e rifiuta troncamenti. Font e
atlas devono corrispondere, inclusi le dimensioni; gli shader personalizzati
devono rispettare l'interfaccia del renderer. La risoluzione dei percorsi non è
un controllo di contenimento per input non fidato (`..` è consentito).

I GLSL canonici vengono compilati da CMake; gli SPIR-V presenti nel checkout
non sono usati dall'installazione. La configurazione delle risorse è locale
all'APP e non modifica la root globale usata dal picker.

## Verifica ripetibile

```sh
python3 tools/validate_app_sdk.py
# Include le finestre Vulkan reali (serve un desktop disponibile):
python3 tools/validate_app_sdk.py --runtime
# Workflow completo dell'editor con input X11/i3 sulle sole finestre possedute:
python3 tools/validate_app_sdk.py --native
```

Il verificatore costruisce Debug/Release per entrambi gli host, installa e
sposta ciascun SDK, copia entrambi gli esempi fuori dal repository, compila i consumer,
poi sposta nuovamente il bundle. In modalità runtime avvia il bundle da un'altra
directory e controlla il risultato con i validation layer nelle build Debug.
Verifica anche i quattro file mancanti, gli override dei nomi dei file e una
root assoluta, la copia risorse senza link e ogni header pubblico separatamente.
La suite comportamentale dell'editor viene inoltre compilata fuori dal checkout
contro l'SDK installato: usa input UI esplicito, renderer deterministico e file
reali per verificare apertura/salvataggio tramite picker, modifica, undo/redo,
conflitti, annullamenti e salvataggio prima di chiudere. È una prova funzionale
distinta sia dagli smoke Vulkan sia dall'input desktop nativo.
Con `--native` verifica in Debug su entrambi gli host focus, click completi,
clipboard sintetica, undo/redo, picker, salvataggio, ridimensionamento, annullamento
della chiusura, salvataggio prima di chiudere e riapertura in un nuovo processo.
Il report distingue queste prove dall'interazione manuale. Non usare il desktop
durante la prova nativa: una perdita di focus inattesa interrompe l'invio di input.

L'aggiunta di `APP.resources` cambia il layout di `APP`: ricompilare libreria e
consumatori insieme. L'API/versione della UI resta invariata; questa modifica
non aggiorna né riattesta gli artefatti della precedente release UI. Le prove
UI sui sorgenti correnti sono archiviate separatamente dalle vecchie release.

L’adattatore legacy `app_loop` con `app_draw` rimane disponibile. Il validatore
collega entrambi i contratti contro SDK installati e controlla che gli eseguibili
a callback non contengano `app_draw`. CTest include i test di veto, errori e lifecycle.
