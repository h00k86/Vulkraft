# Distribuzione sorgenti GraphicsApp 0.8.0

Il pacchetto comprende codice C, header pubblici e privati necessari alla build,
entrambi gli host GLFW/SDL2, host headless, shader GLSL, atlas/font e licenza
OpenSans, esempi e test. Gli SPIR-V vengono ricompilati da `glslc`.
Non comprende directory di lavoro, binari, credenziali, evidenze storiche o narrativa.

La lista di inclusione è esplicita in `tools/graphics_app_sources.json`: aggiungere
un file nel checkout non lo aggiunge automaticamente alla distribuzione. La lista
stessa è inclusa nell'archivio. Il packager rifiuta duplicati, percorsi fuori
ambito e sorgenti symlink o esterne al checkout.

```sh
python3 tools/package_app.py
python3 tools/check_app_package.py --output-dir evidence/app-package
```

Gli artefatti risultanti sono:

- `build/graphics-app-0.8.0-source.tar.gz`;
- `build/graphics-app-0.8.0-source.tar.gz.sha256`;
- `build/graphics-app-0.8.0-manifest-sha256.json`.

Il manifest registra SHA-256 dei byte di ogni sorgente. I membri tar sono ordinati,
file regolari 0644, UID/GID 0 senza nomi proprietario. Tar e gzip usano timestamp
`SOURCE_DATE_EPOCH`, default 0; gzip non registra il nome del file. Con gli stessi
sorgenti, epoch e implementazione Python/zlib si ottengono gli stessi byte,
anche dopo cambi di mtime o permessi; non si promette compressione identica
tra versioni diverse della toolchain.

Il verificatore controlla metadati, checksum, manifest e lista dei membri prima
di estrarre. Rifiuta duplicati, link, path traversal e contenuti alterati. Esegue
poi due repack, uno dopo modifiche ai metadati delle sorgenti estratte, e confronta
i byte. Infine esegue il validatore SDK **dalla copia estratta**, che costruisce
GLFW/SDL2 Debug/Release, installa e sposta gli SDK, compila header isolati e consumer
esterni e prova il workflow UI/controller dell'editor con file reali. Il checkout
originale non fornisce codice o risorse a quelle build.

`--runtime` aggiunge smoke Vulkan dei bundle spostati; `--native` aggiunge anche
input X11/i3 sulle sole finestre possedute. Il secondo richiede un desktop libero,
xdotool e Pillow. Le prove funzionali con renderer deterministico restano distinte
da rendering Vulkan, input desktop e revisione manuale.

## Un solo comando per il progetto

```sh
python3 tools/validate_project.py --output-dir evidence/project-validation
# Aggiunge finestre Vulkan, senza input desktop:
python3 tools/validate_project.py --runtime --output-dir evidence/project-runtime
# Aggiunge anche workflow nativi:
python3 tools/validate_project.py --native --output-dir evidence/project-native
```

La validazione generale mantiene i controlli Make, sanitizer, UI source/SDK,
smoke software e benchmark; aggiunge gli stage `app-source` e `app-sdk`.
Il secondo verifica il pacchetto e richiama il validatore SDK dalla copia estratta,
evitando una seconda matrice equivalente sui sorgenti del checkout.
I pacchetti creati durante la validazione vanno in `ui-source/` e `app-source/`
sotto la directory delle evidenze; non sovrascrivono gli artefatti precedenti
in `build/`. Il finalizzatore UI scrive anche il riepilogo accanto all'archivio
verificato, preservando i riepiloghi storici di `build/`.

Il report principale è `validation.json`; `app-sdk/package-verification.json`
collega il risultato all'hash dell'archivio e del manifest. Il report annidato
`app-sdk/sdk/report.json` conserva tutti gli stage dell'SDK e le verifiche mancanti.
Una fase fallita o interrotta non viene dichiarata PASS.

## Compilazione dell'archivio

```sh
tar -xzf graphics-app-0.8.0-source.tar.gz
cmake -S graphics-app-0.8.0 -B app-build -DCMAKE_BUILD_TYPE=Release \
  -DGRAPHICS_APP_WITH_VULKAN=ON -DGRAPHICS_APP_PLATFORM=sdl2
cmake --build app-build -j4
cmake --install app-build --prefix /tmp/graphics-app-sdk
```

Dipendenze e contratto pubblico: [GRAPHICS_APP_SDK.md](GRAPHICS_APP_SDK.md) e
[GRAPHICS_APP_API.md](GRAPHICS_APP_API.md). La distribuzione sorgenti non include
driver Vulkan o librerie native. L'SDK UI ha versione 0.6.0;
preparare un archivio verificato non pubblica automaticamente una release.
