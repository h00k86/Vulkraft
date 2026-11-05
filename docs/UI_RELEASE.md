# Generare e verificare una versione di riferimento

## Sorgenti e versione

I comandi Make e la validazione completa richiedono il checkout del progetto.
L’SDK sorgente estratto usa CMake e può rigenerare il proprio archivio con Python.

Modificare soltanto `include/ui/version.h` per cambiare versione, seguendo
[UI_COMPATIBILITY.md](UI_COMPATIBILITY.md), e aggiungere una voce a
`CHANGELOG.md`. Non cambiare manualmente nomi di archivi o versione CMake.

```sh
python3 tools/ui_version.py
make ui-package
```

Il pacchetto viene scritto in `build/ui-framework-<version>-source.tar.gz`,
accompagnato da `.tar.gz.sha256` e `ui-framework-<version>-manifest-sha256.json`.
Il manifest è presente anche nell'archivio. I file sono selezionati da una lista
esplicita: il packager non raccoglie automaticamente file dal checkout.

Timestamp gzip/tar, ordine, permessi, UID/GID e nomi dei proprietari sono
normalizzati. Il timestamp predefinito è zero; `SOURCE_DATE_EPOCH` permette
un valore esplicito da 0 a 4294967295. L'archivio gzip non contiene il nome del
file di output. Stessi byte dei sorgenti, stesso epoch e stessa implementazione
Python/zlib producono lo stesso SHA-256; non si promette identica compressione
tra implementazioni o versioni diverse di zlib.

```sh
python3 tools/package_ui.py --output-dir /tmp/ui-sdk-repacked
```

Il packager e il lettore della versione sono inclusi nell'archivio: lo stesso
comando funziona dalla copia estratta. I permessi dei sorgenti non influenzano
l'archivio; tutti i membri sono normalizzati a 0644 e gli script si avviano
con `python3`. I file compilati restano specifici della piattaforma/toolchain.

## Verifica dell'SDK

```sh
make test-ui-package
```

Il controllo confronta tutti i byte con il manifest e verifica metadati,
checksum e riproducibilità dalla copia estratta, anche dopo aver variato
timestamp e permessi dei sorgenti. Costruisce core e SDK completo, esegue CTest,
sposta l'installazione e compila/esegue consumatori esterni. Un consumatore
confronta versione CMake, macro degli header e versione del core collegato;
le richieste di versioni diverse devono essere rifiutate.

## Riepilogo associato all'archivio verificato

```sh
python3 tools/validate_project.py --output-dir evidence/ui-release/validation
python3 tools/finalize_ui_release.py --validation-dir evidence/ui-release/validation
```

La validazione conserva l'archivio in `ui-source/` sotto la cartella delle
evidenze, senza sostituire gli artefatti precedenti in `build/`. Il controllo
SDK accetta anche `--archive-dir` per indicare esplicitamente questa posizione.
Il secondo comando genera `ui-framework-<version>-release.json` accanto
all'archivio verificato (per il comando sopra, in
`evidence/ui-release/validation/ui-source/`) solo se
le fasi richieste sono tutte PASS, l'archivio è quello verificato dal controllo
SDK e i sorgenti selezionati coincidono ancora con il manifest. Il riepilogo
riporta versione, hash, fasi e limiti delle verifiche. Non modifica o pubblica
repository, tag o release remote.

I report precedenti privi di `ui_archive_dir` mantengono la convenzione `build/`.

`--previous-native-report <report.json>` può aggiungere un rapporto nativo già
esistente: viene identificato come evidenza precedente, non come prova
rieseguita sull'archivio corrente. Per una nuova prova sul desktop usare
`validate_project.py --native`, lasciando liberi mouse e tastiera.

Il riepilogo delle verifiche conserva la provenienza dei risultati; non è un
artefatto che si pretende identico dopo una nuova esecuzione dei test. La
riproducibilità byte per byte riguarda l'archivio sorgente con gli stessi input.
