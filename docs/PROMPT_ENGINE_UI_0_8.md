# Messaggio per l'agente del motore: aggiornamento UI 0.8.0

Aggiorna la copia vendorizzata a UIFramework 0.8.0 usando l'archivio
`/home/h00k/CODING/C/GAME/VULKAN_3D/GRAPHICS_APP_BASE/build/ui-framework-0.8.0-source.tar.gz`
e il relativo checksum/manifest. L'archivio 0.7.0 che hai importato resta intatto.
Il checkout può avere modifiche non committate: non usare il vecchio HEAD come
se contenesse la nuova versione. Importa lo snapshot completo e ricompila core,
picker e host; aggiorna VERSIONE.md con provenienza e checksum effettivi.

Leggi `docs/UI_PICKER_FOCUS.md`. Mantieni il tuo pannello e i servizi documenti:
questa consegna modifica soltanto componenti e contratti del toolkit.

1. Nel picker del pannello Sorgente usa `extensions`/`extension_count` per
   `.vert`, `.frag`, `.comp`, `.glsl`, `.c`, `.h`, `.txt`. Il vecchio filtro
   singolo rimane disponibile per gli altri picker. Non togliere la validazione
   host del contenuto. Il confronto dei suffissi è sensibile alle maiuscole.
2. In Salva come, scegli `default_extension` dal suffisso del documento corrente
   oppure chiedilo all'utente. Non scegliere automaticamente `.vert` o `.c` per
   tutti i file: la prima voce della lista non è un default implicito.
3. Per tornare al documento dopo il picker aperto da menu/pulsante, imposta
   `request.return_focus` all'ID stabile dell'editor e `request.resume_editing=true`.
   Per le conferme disegnate dall'host chiama una volta, dentro il primo begin
   riuscito, `ui_modal_set_return_focus(id_modale, id_documento, true)` e controlla
   il risultato. Il documento deve risultare registrato nello scope padre.
4. Senza override il core torna al widget che aveva il focus all'apertura;
   riprende una sessione testo/numerica attiva senza perdere cursore, selezione,
   undo/redo o bozza. Non aggiungere un `ui_text_input_focus` a ogni frame e non
   usare ID o strutture private per aggirare la restituzione del focus.
5. Prova filtri, Salva come con suffisso ammesso, senza suffisso e con suffisso
   errato; apertura da menu e ritorno al testo dopo Annulla/Conferma/Esc, dopo
   modale annidata e dopo perdita focus finestra. Il tasto/clic di chiusura non
   deve modificare il documento né attivare comandi della scena.
6. Esegui suite e collaudo del motore, nelle due dimensioni già usate. Conserva
   la clipboard dell'utente durante il collaudo. Nessun salvataggio di livelli
   o asset reali per dimostrare il funzionamento. Distingui scenario automatico,
   controllo visivo e tastiera reale. Consegna commit e risultati; niente merge
   o push senza autorizzazione specifica già presente.

Non aggiungere ora compilazione asincrona, nuovo scripting, anteprime GLB o
migrazione della Scena: restano attività separate dal pannello e dal toolkit.
