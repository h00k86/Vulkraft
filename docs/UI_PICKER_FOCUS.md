# Filtri multipli e ritorno del focus

UIFramework 0.8.0, GraphicsApp 0.10.0. Core e modulo picker vanno ricompilati
insieme agli header e all'host. Nessuna modifica al game engine è inclusa qui.

## Picker: più estensioni

```c
static const char *const source_extensions[] = {
    ".vert", ".frag", ".comp", ".glsl", ".c", ".h", ".txt"
};
UIFilePickerRequest request = {
    .mode = UI_FILE_PICKER_OPEN,
    .root_directory = source_root,
    .extensions = source_extensions,
    .extension_count = sizeof(source_extensions)/sizeof(source_extensions[0]),
    .return_focus = "source/document-42",
    .resume_editing = true
};
ui_file_picker_open("source/open", "Apri sorgente", &request);
```

I nuovi campi sono in coda a `UIFilePickerRequest`; inizializzare sempre la
struttura, anche con `{0}`. La lista viene copiata da `ui_file_picker_open`:
array, suffissi e richiesta non devono sopravvivere alla chiamata. Un suffisso
comprende il punto iniziale, per esempio `.frag`, ed è lungo al massimo 23 byte.
Massimo 16 suffissi. Non sono espressioni glob: niente `*.frag`, separatori di
percorso, spazi o liste separate da `;`. Sono ammessi suffissi composti come
`.tar.gz`. Confronto **sensibile alle maiuscole**, come il filtro precedente.
Per accettare anche `.FRAG`, inserirlo nella lista.

`extension_count > 0` sostituisce il vecchio campo `extension`.
Con `extension_count == 0` resta il filtro singolo precedente: NULL/stringa vuota
significa tutti i file. Le directory restano visibili indipendentemente dal
filtro; ricerca e contenimento nella root conservano il comportamento esistente.
Una richiesta con lista NULL e conteggio positivo, conteggio fuori limite,
suffisso invalido/troppo lungo o default non ammesso viene ignorata **senza
chiudere un picker già aperto**. Non viene allargato il filtro né troncato il
suffisso per adattarlo alla cache. Il filtro non dimostra il formato del file:
l'host continua a validare contenuto, dimensione e permessi dopo la scelta.

### Salvataggio

La lista nuova non sceglie automaticamente la prima estensione. Per SAVE:

- Nome con suffisso ammesso: viene mantenuto.
- Nome senza punto: si aggiunge `default_extension` **solo se esplicito**.
- Nome con suffisso non ammesso: errore nel picker, nessuna sostituzione e nessun
  nome trasformato silenziosamente in `esempio.txt.frag`.
- Nome senza suffisso e senza default: errore, scegliere esplicitamente il suffisso.
- Il default (NULL o stringa vuota: assente) deve appartenere alla lista (oppure corrispondere al filtro singolo,
  se presente). Nessun default deve essere scelto dal toolkit in base al contenuto.

Per il pannello Sorgente conviene conservare il suffisso del documento corrente,
oppure chiedere all'utente quale tipo salvare. Il picker restituisce il percorso:
non crea/sovrascrive il file. Il vecchio filtro `extension=".txt"` senza i nuovi
campi continua ad aggiungere quel suffisso secondo il contratto precedente.

## Modali: focus di navigazione ed editing

All'apertura il core ricorda il widget che aveva il focus e sospende la sua
sessione di editing, se attiva. Alla chiusura torna a quel widget e riprende
l'editing, con cursore, selezione, undo/redo e scroll del testo conservati.
Il picker resta una modale padre anche mentre mostra la conferma di sovrascrittura:
Annulla torna al picker senza riprendere prematuramente l'editing del documento.
I campi monoriga mantengono la baseline di Esc; i numerici mantengono la bozza
non ancora confermata. Lo stato sospeso non cattura la tastiera: la possiede
la modale. Vale anche per dialoghi annidati e modali non più presentate dall'host.

Gli eventi usati per chiudere restano associati al dialogo, senza trasformarsi
in caratteri/clic sul documento. L'anello di focus segue la modalità di input
corrente: chiusura da mouse o da tastiera mantiene le regole esistenti.

Un menu o pulsante può prendere il focus prima dell'apertura. In questo caso
il default è tornare a quel pulsante, non indovinare quale documento l'utente
intende modificare. Per scegliere il destinatario, durante una modale aperta:

```c
if (ui_modal_begin("confirm", "Conferma", font, 400, 220, &open,
                   &x, &y, &w, &h)) {
    ui_modal_set_return_focus("confirm", "source/document-42", true);
    /* contenuto della modale */
}
ui_modal_end("confirm", open);
```

Il target deve essere un ID valido **già registrato nel contesto e nello scope
padre** (frame corrente o precedente). Un widget interno alla modale non è un
ritorno valido. Il setter non cambia il focus immediatamente; restituisce false
senza cambiare il target per ID, modale o widget non validi.

Con `resume_editing=true`, il target deve essere un campo testo, editor multilinea
o campo numerico. Un testo inattivo può riprendere dalla selezione conservata,
creando una baseline di modifica nuova; un numerico deve avere già una sessione
attiva/sospesa da riprendere. Con false torna solo il focus di navigazione,
senza riaprire un editor al primo evento di testo. Invocare il setter una volta
all'apertura, oppure quando cambia intenzionalmente il target.

Il picker espone `return_focus` (NULL/stringa vuota: ritorno ordinario) e
`resume_editing` perché disegna internamente
la propria modale. Li copia all'apertura e applica il setter quando il target
risulta registrato. Se non è valido, resta il ritorno ordinario al widget di
origine. Usare esattamente l'ID dato a `ui_text_editor`; nessun accesso allo stato
privato del toolkit.

Perdita del focus finestra e `ui_release_input_capture()` cancellano la ripresa
pendente. Se il target scompare o viene disabilitato, la validazione dei widget
sceglie il prossimo destinatario disponibile senza lasciare un editor nascosto
in cattura. Reset/destroy liberano anche le sessioni sospese. L'host deve comunque
sospendere il polling delle proprie scorciatoie quando la UI cattura l'input e
registrare le regioni prima del proprio update.
