# Compatibilità del framework UI

## Versione e identità del pacchetto

`include/ui/version.h` è l'unica fonte della versione corrente. CMake,
strumenti Python e archivio sorgente leggono le tre componenti decimali.
`UI_VERSION_STRING` identifica gli header usati nella compilazione;
`ui_version_string()` identifica il core effettivamente collegato, anche senza
un contesto. Il puntatore restituito ha durata statica e non va liberato.
`UIFramework_VERSION` espone la versione nei consumatori CMake.

La versione è accompagnata dal manifest e dall'hash dell'archivio: un vecchio
snapshot di sviluppo con lo stesso numero non sostituisce il pacchetto di
riferimento. Conservare il checksum insieme all'SDK usato dall'applicazione.

## API pubbliche e implementazione privata

Il contratto pubblico comprende gli header UI installati (`ui.h`, `version.h`,
`error.h`, `renderer.h`, `draw_list.h`, `file_picker.h`), il backend opzionale
`backends/ui_sdl2.h` e i tipi di supporto inclusi da questi header. I target
esportati sono `UIFramework::core`, `UIFramework::picker` e `UIFramework::sdl2`.
Il picker e SDL richiedono il relativo componente, anche quando un header è
disponibile nell'installazione. Proprietà e durata delle risorse sono descritte
in [UI_API.md](UI_API.md) e negli header delle rispettive API.

I file in `src/`, i nomi `*_internal.h`, layout fisico dei contesti, capacità
interne delle cache e strumenti di benchmark non costituiscono API pubbliche.
Non includere header privati o collegare direttamente funzioni interne.
L'host APP/GLFW/Vulkan del checkout non è parte del pacchetto UI standalone.

## Regole per le versioni 0.x

- Una patch incrementa PATCH per correzioni compatibili con le chiamate
  pubbliche documentate. Non cambia firme, proprietà delle risorse o significato
  degli esiti; la correzione di un comportamento errato viene descritta nel changelog.
- Un'aggiunta pubblica o una modifica incompatibile incrementa MINOR e azzera
  PATCH. Per una modifica incompatibile il changelog indica come migrare.
- I dettagli privati possono cambiare; non sono garantiti sizeof del contesto,
  indirizzi di memoria tra reset o prestazioni identiche tra versioni.
- Ricompilare applicazione e librerie statiche dopo ogni aggiornamento. Non è
  garantita la compatibilità ABI tra versioni, nemmeno per una patch. Usare
  header e librerie dello stesso pacchetto; l'uguaglianza della stringa versione
  non sostituisce il controllo del manifest per vecchi snapshot di sviluppo.

CMake conserva `ExactVersion`: le richieste con un numero diverso non vengono
considerate automaticamente compatibili. Aggiornare esplicitamente la versione
richiesta dal progetto consumatore dopo aver letto il changelog.

## Garanzie operative e limiti

Le API UI con stato richiedono contesto corrente, thread principale e assenza
di rientranza dalle callback. Non viene aggiunta sicurezza per l'uso concorrente.
La ricompilazione non rimuove questi vincoli né quelli di durata di font,
texture, backend e liste differite.

Le verifiche disponibili coprono Linux/X11 sul sistema di sviluppo, renderer
simulati, SDL2 e l'host Vulkan del checkout. Non certificano Windows, macOS,
Wayland, altre GPU, shaping/grafemi/IME, accessibilità o LeakSanitizer.
Il riepilogo di una versione distingue verifiche automatiche correnti,
evidenze native precedenti e aspetti non verificati.

## Migrazione a 0.2.0

Le etichette dei widget restano nella forma fornita dall'applicazione;
`ui_set_sentence_case(true)` abilita esplicitamente una normalizzazione ASCII
per le etichette tutte maiuscole nel contesto corrente. La preferenza è isolata
per contesto e il reset la disattiva. Non modifica i buffer di testo dell'utente.

Esc chiude la modale attiva anche con un editor focalizzato. Gli overlay
menu/dropdown aperti mantengono precedenza e consumano una singola pressione.
La responsabilità di annullare i dati del dialogo resta dell'applicazione.

Autofocus e ripristino del focus non implicano input da tastiera; l'anello
segue il metodo di input attivo. Dettagli in
[UI_INTEGRATION_BEHAVIOR.md](UI_INTEGRATION_BEHAVIOR.md). Ricompilare consumatori
e librerie con la stessa versione; i package CMake richiedono la versione esatta.
