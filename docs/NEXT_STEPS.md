# Prossimi passi per GRAPHICS_APP_BASE

Aggiornamento: 5 novembre 2026.

Il core è indipendente da GLFW, con contesto opaco, stato suddiviso per
responsabilità e moduli compilati separatamente. Navigazione, editing UTF-8,
clipboard, undo/redo, tab/splitter, menu gerarchici, picker e applicazione editor
sono già presenti. Contratti in [UI_API.md](UI_API.md) e
[UI_INTERNAL_ARCHITECTURE.md](UI_INTERNAL_ARCHITECTURE.md).

## Consolidamento

La verifica unificata, il benchmark e le prove native sono descritti in
[PROJECT_VALIDATION.md](PROJECT_VALIDATION.md). Gli esiti effettivi, compresi
i limiti delle misure, sono in `evidence/consolidation/`.

Usare `make validate` dopo modifiche al framework. Usare `make validate-native`
quando cambiano host, input, focus, finestre o composizione dell'editor.
Tenere distinti compilazione Vulkan, input nativo automatizzato, osservazione
visiva e prestazioni GPU.

## Stato delle migliorie

L'arena riutilizzabile per i testi della draw list è implementata: conserva
capacità tra frame e azzera le allocazioni delle etichette nei carichi già
riscaldati. Semantica delle copie, limiti e failure injection sono coperti dai
test. Evidenze e confronto in `evidence/ui-text-arena/`.

Il benchmark dell'editor e il dettaglio della memoria sono implementati:
`build/ui-editor-benchmark.out` misura editing, cronologia, scroll, menu e
picker, su entrambi i percorsi, e scompone la memoria dei domini senza esporre
il layout del contesto. Evidenze in `evidence/ui-editor-profile/`.

Le copie delle opzioni dei dropdown ora vengono allocate solo all'apertura
del popup, per le righe visibili. Il buffer cresce da 8 fino a 128 righe,
viene riusato dalla cache e viene liberato al reset/destroy. Le voci del
registro mantengono indirizzi stabili. Verifiche e confronto con il profilo
precedente sono in `evidence/ui-dropdown-storage/`.

Anche gli owner degli eventi e i payload dei menu sono ora allocati su
richiesta. Il routing mantiene gli ID completi e riserva solo gli slot del
batch reale, prima di aprire il frame. I menu conservano metadati stabili e
allocano separatamente le voci di radice, figli e dichiarazioni. Evidenze dei
tre punti (routing, menu, nuova misura) in `evidence/ui-routing-menu/`.

La diagnostica tipizzata distingue ora input, stato, limiti e memoria nei
punti documentati in [UI_ERRORS.md](UI_ERRORS.md). Il rifiuto del frame resta
atomico. Le suite di affidabilità comprendono errori di allocazione e sessioni
riproducibili di 27.648 frame, con reset, due contesti e rendering differito.
Esiti automatici e tentativi nativi di questo ciclo sono raccolti in
`evidence/ui-reliability/`.

## Sessioni della composizione reale

La suite `config_editor_session_test` concatena ora 72 workflow dell'editor,
con editing UTF-8, due cronologie, salvataggi falliti indotti, conflitti esterni,
copie e riaperture. Passano debug, ASan/UBSan e SDK Release; la memoria osservata
ritorna a zero dopo ciascun blocco di quattro workflow.
Evidenze in `evidence/editor-session/`.

La verifica nativa completa è ora PASS su SDL2 e Vulkan, compresa la modifica
e il salvataggio dopo la riapertura in un nuovo processo. Sono verificati
anche focus, clipboard, cronologia, picker, resize e chiusura; Vulkan usa il
validation layer e i log non riportano errori rilevati. Rapporto e screenshot
in `evidence/editor-session/native-confirmed-idle/`. I precedenti tentativi
interrotti per focus esterno restano conservati. La prova automatizzata X11/i3
è distinta da una sessione manuale e dalla verifica su altre piattaforme.

## SDK di riferimento

La versione 0.1.0 è centralizzata in `include/ui/version.h`, condivisa da
CMake, API pubblica e packager. Un'applicazione può confrontare
`UI_VERSION_STRING` con `ui_version_string()` del core collegato.
Changelog, regole di compatibilità e riproduzione sono in `CHANGELOG.md`,
[UI_COMPATIBILITY.md](UI_COMPATIBILITY.md) e [UI_RELEASE.md](UI_RELEASE.md).

L'archivio ha metadati normalizzati, manifest e checksum; il riepilogo delle
verifiche è legato all'hash controllato dall'SDK e rifiuta sorgenti modificati
successivamente. Verifica completa PASS, 18 test core e 29 completi, con prove
sulla riproducibilità e consumatori esterni. Artefatti in `build/`, evidenze in
`evidence/ui-release/`. Il riepilogo distingue le prove native precedenti dalle
verifiche automatiche correnti.

## Secondo host Vulkan

Il backend SDL2 implementa ora la piattaforma APP e il ponte Vulkan.
`PLATFORM=glfw` e `PLATFORM=sdl2` compilano gli stessi esempi in percorsi
separati. Passano le build debug/release, i test dei collector e delle
risorse, dodici scene runtime con validation layer e la sequenza nativa
completa dell'editor su entrambi gli host. Dettagli in
[PLATFORM_SDL2_VULKAN.md](PLATFORM_SDL2_VULKAN.md), evidenze in
`evidence/platform-sdl2/`.

## SDK applicativo completo

`GraphicsApp::application` esporta ora il ciclo APP e il renderer Vulkan per
progetti CMake esterni, con un host GLFW oppure SDL2 per installazione.
`APP.resources` configura root, metadati/atlas font e SPIR-V senza modificare
la root globale. L'esempio esterno porta le risorse accanto all'eseguibile.
Istruzioni e verifica ripetibile in [GRAPHICS_APP_SDK.md](GRAPHICS_APP_SDK.md);
evidenze in `evidence/app-sdk/`.

## Prossima direzione

La validazione generale include ora il pacchetto sorgenti GraphicsApp: archivio
riproducibile e ricostruzione degli SDK/consumer dai soli file distribuiti.
I workflow nativi degli editor esterni GLFW e SDL2 sono stati completati, incluso
Open picker, undo/redo, salvataggio e chiusura con modifiche pendenti. Nuove
esecuzioni complete restano opt-in tramite `--native`.

L'editor di configurazione è ora un consumer autonomo dell'SDK applicativo 0.3.0:
il progetto CMake dell'esempio usa solo header e librerie installati. Il validatore
SDK include compilazione dei 28 header pubblici separati, aggiornamento delle
risorse senza link, editor esterno e prove native opt-in con `--native`.
La superficie pubblica è descritta in [GRAPHICS_APP_API.md](GRAPHICS_APP_API.md).

Il passo utile è usare questa base per un'applicazione concreta più ampia,
scegliendo un singolo workflow completo. I limiti osservati lì guideranno
le prossime modifiche di architettura. Le misure attuali non giustificano
ulteriori suddivisioni dei moduli o un nuovo sistema di ricerca degli ID.

IME/grafemi, accessibilità, altre piattaforme e multi-finestra richiedono
obiettivi e contratti propri, scelti in base all'applicazione. Le riserve
fisse della navigazione meritano attenzione se un caso reale richiederà
molti contesti contemporanei.

## Integrazione applicativa a callback

GraphicsApp 0.3.0 aggiunge `app_run`, `AppCallbacks` e `app_request_close`.
Editor ed esempio minimo conservano lo stato nel main e lo passano tramite
`user_data`; la chiusura dell'editor passa dal controller senza sostituire
callback piattaforma. `app_loop`/`app_draw` restano disponibili come adattatore
separato. Contratto in `GRAPHICS_APP_API.md`, prove in `evidence/app-callbacks/`.
