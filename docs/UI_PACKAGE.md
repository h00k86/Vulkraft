# Package UI Framework

Il Makefile originale resta disponibile. CMake costruisce e installa solo il
framework UI indipendente; i moduli opzionali sono disabilitati per default.

```sh
cmake -S . -B build/ui-package -DCMAKE_BUILD_TYPE=Release \
  -DUI_WITH_PICKER=ON -DUI_WITH_SDL2=ON
cmake --build build/ui-package -j4
ctest --test-dir build/ui-package --output-on-failure
cmake --install build/ui-package --prefix "$PWD/build/ui-install"
```

Per il solo core omettere le due opzioni ON. SDL non viene cercato se il backend
è disabilitato. `UI_BUILD_EXAMPLES=OFF` e `BUILD_TESTING=OFF` disabilitano esempi
e test. Nessun download automatico o installazione di dipendenze.

Un progetto consumatore usa:

```cmake
find_package(UIFramework 0.4.0 EXACT REQUIRED COMPONENTS core)
add_executable(my_app main.c)
target_link_libraries(my_app PRIVATE UIFramework::core)
# Se installati, sono disponibili anche UIFramework::picker e UIFramework::sdl2.
```

Configurare il consumatore con `-DCMAKE_PREFIX_PATH=/percorso/installazione`.
Usare `COMPONENTS core picker sdl2` se necessari: componenti non installati vengono
rifiutati. Anche da un SDK completo, `COMPONENTS core` o `COMPONENTS core picker`
carica soltanto i componenti richiesti, senza cercare pkg-config o SDL.
`OPTIONAL_COMPONENTS sdl2` permette di usare il core anche quando le dipendenze SDL
non sono disponibili: controllare `UIFramework_sdl2_FOUND` prima di collegarlo.
Senza una lista di componenti vengono caricati tutti quelli installati, mantenendo
il comportamento originale. Chiamate successive possono aggiungere componenti.
Le librerie statiche installate sono specifiche della piattaforma/build corrente.

Header pubblici, librerie, esempi e documentazione vengono installati senza header
privati, APP o renderer Vulkan. Il font OFL è installato con SDL sotto
`share/ui-framework/fonts`; avviare la demo installata con `--font` e `--root`:

```sh
build/ui-install/bin/ui_reference_sdl2 \
  --font build/ui-install/share/ui-framework/fonts/OpenSans-Italic.ttf --root .
```

`make ui-package` produce inoltre `build/ui-framework-<version>-source.tar.gz` con
sorgenti selezionati e manifest SHA-256, ricompilabile fuori dal checkout. Non
include build, evidence, asset estranei, credenziali o directory nascoste. Non
aggiunge una nuova licenza al codice del progetto; il font conserva la sua OFL.

Il package usa target esportati e configurazione rilocabile secondo la
[guida ufficiale CMake](https://cmake.org/cmake/help/latest/guide/importing-exporting/index.html).
Il contratto corrente è descritto in `UI_API.md`. La versione 0.1.0 è una base
integrabile, non una promessa di ABI immutabile o portabilità già verificata su
Windows/macOS.

`make test-ui-package` ripete la verifica: estrae l’archivio in una directory
temporanea, controlla gli hash, compila core/full, esegue CTest, sposta l’SDK e
compila/esegue consumatori esterni. Verifica inoltre core/picker senza ricerca di
pkg-config, SDL opzionale o richiesto ma assente, componenti sconosciuti e chiamate
successive a `find_package`. Log in `evidence/ui-sdl2/`.

La versione viene letta da `include/ui/version.h`, anche da CMake e dagli
strumenti del package. L'archivio è normalizzato e accompagnato da checksum
e manifest esterno. Il controllo SDK lo rigenera dalla copia estratta e
confronta i byte dopo modifiche ai soli timestamp/permessi; verifica anche
la versione di un consumatore esterno e il rifiuto di versioni differenti.
Riproduzione e riepilogo verifiche in [UI_RELEASE.md](UI_RELEASE.md);
regole di aggiornamento in [UI_COMPATIBILITY.md](UI_COMPATIBILITY.md).
I test degli strumenti di release usano Python 3 quando disponibile nella
build CMake; non aggiungono una dipendenza Python alle librerie installate.
