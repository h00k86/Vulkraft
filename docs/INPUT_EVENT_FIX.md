# Eventi tastiera tra frame — verifica isolata e correzione

**Aggiornamento:** il successivo [supporto UTF-8](UTF8_SUPPORT.md) sostituisce
il limite ASCII descritto in questo resoconto storico.


## Distinzione dalle prove sul desktop

L'utente ha segnalato che stava spostando il mouse durante l'automazione.
Le anomalie delle prove precedenti sul desktop condiviso non consentono di
attribuire ogni carattere mancante al codice: focus, geometria e puntatore
potevano essere interferiti. Quelle prove restano storiche.

Prima di modificare il codice è stato quindi eseguito un nuovo controllo:
`tests/glfw_key_event_probe.c` crea una finestra GLFW/X11 **nascosta** e invia
con XSendEvent gli eventi soltanto a quella finestra. Non muove il mouse,
non cambia il focus del desktop e non invia tasti alle altre applicazioni.
Il campo UI viene mantenuto focalizzato dal test; disegno e misura testo
sono stub, mentre GLFW, la coda X11 e il codice del widget sono reali.

| Caso | Prima | Dopo |
| --- | --- | --- |
| Pressione, frame, rilascio, frame | `z`, PASS | `z`, PASS |
| Pressione e rilascio prima dello stesso frame | testo vuoto, FAIL | `z`, PASS |

In entrambi i casi la callback osservatrice riceve una pressione e un
rilascio e il campo mantiene il focus. Questo isola un difetto distinto dal
movimento del mouse: il polling di `glfwGetKey` vedeva soltanto RELEASE dopo
che GLFW aveva elaborato la coppia di eventi.

Output osservati conservati in
[`before.txt`](../evidence/input-event-fix/before.txt) e
[`after.txt`](../evidence/input-event-fix/after.txt).
Il probe è terminato rispettivamente con exit 1 e exit 0.

## Modifica

- Nuovo modulo `input/keyboard`: callback tasti, caratteri e focus; eventi
  conservati nell'ordine di arrivo e pubblicati una volta per frame UI.
- I campi testo e numerici consumano la sequenza completa, comprese pressioni
  ripetute, repeat e modificatori associati ai singoli comandi. Le lettere
  non vengono più ricostruite percorrendo i keycode in ordine alfabetico.
- Il testo ASCII proviene dalla callback caratteri di GLFW: layout tastiera,
  maiuscole e punteggiatura vengono rispettati per i caratteri supportati.
- Enter/Escape terminano l'editing; gli eventi successivi dello stesso batch
  non passano accidentalmente a un altro campo. Gli eventi non consumati
  scadono al frame seguente; la perdita del focus cancella il batch e il focus
  degli editor.
- `app_init` registra le callback, `app_shutdown` le ripristina e azzera la
  coda. La scena diagnostica concatena la propria callback osservatrice alla
  callback preesistente, senza sostituire il raccoglitore di input.

Con un loop personalizzato: chiamare `input_keyboard_register(window)` prima
del polling, `ui_begin_frame` una sola volta dopo il polling e
`input_keyboard_unregister(window)` prima di distruggere la finestra.
La registrazione possiede i tre slot callback fino alla deregistrazione,
che ripristina le callback originarie. Eventuali wrapper successivi devono
concatenarsi alla callback restituita da GLFW e vengono rimossi allo shutdown.

## Verifiche

Log in [`evidence/input-event-fix`](../evidence/input-event-fix/):

- Build debug e release: PASS, nessun warning/error di compilazione.
- `make test`: PASS, 12 eseguibili e checker dei confini architetturali.
- `make test-sanitize`: PASS, ASan/UBSan. LeakSanitizer disabilitato.
- Nuove regressioni: coppia press/release prima del consumo, ordine `zaz`,
  ripetizioni, comandi intercalati ai caratteri, Ctrl+A, Backspace/repeat,
  Enter e keypad Enter, float, INT_MAX, focus perso, nessun replay, overflow
  e ripristino delle callback.
- Probe reale GLFW/X11 nascosto: PASS dopo la correzione, con controllo
  positivo e confronto prima/dopo.
- `git diff --check`: PASS.

Non è stata ripetuta l'automazione del desktop condiviso, né viene dichiarata
una nuova verifica Vulkan completa. Il target nascosto è opt-in:

```sh
make build/glfw-key-event-probe.out
./build/glfw-key-event-probe.out
```

## Limiti espliciti

La coda contiene fino a 1024 eventi tra due consumi. Se supera il limite,
segnala l'errore e scarta l'intero batch liberando il focus, invece di applicare
una sequenza troncata di comandi. Il test di overflow provoca intenzionalmente
quel messaggio; non è un errore inatteso della suite.

Il contratto del testo resta ASCII: questa modifica non introduce editing
UTF-8/IME completo o clipboard. La selezione Ctrl+A conserva il comportamento
preesistente di svuotamento del campo. Il modulo segue l'architettura attuale
con una sola finestra attiva e chiamate sul thread principale.
