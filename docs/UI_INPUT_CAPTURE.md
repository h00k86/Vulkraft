# Cattura dell'input e moduli overlay

`ui_capture.c` centralizza la politica di input, indipendentemente dal backend.
Menu, dropdown e modali sono compilati rispettivamente in `ui_menu.c`,
`ui_dropdown.c` e `ui_modals.c`. Registri e geometrie restano posseduti dal
contesto UI selezionato: non esistono copie dello stato nei nuovi moduli.
Le firme pubbliche restano invariate.

## Ordine e proprietà degli eventi

- Il frame prepara gli snapshot dei dropdown e dei menu prima di assegnare
  il click. Tra dropdown aperti con geometria valida vince quello disegnato
  per ultimo nel frame precedente. Un menu attivo ha precedenza sui dropdown.
- Un click che chiude un popup resta rivendicato fino al frame seguente;
  non preme un controllo sottostante, neppure se dichiarato dopo la chiusura.
- I controlli ordinari rispettano clip, scope modale, annullamento per perdita
  del focus e regole di pressione/rilascio condivise. Il drag può proseguire
  fuori dal rettangolo solo dopo una pressione valida.
- Il menu aperto blocca sia nuove pressioni dei widget ordinari sia il loro
  instradamento della tastiera. Un campo già a fuoco non modifica il testo
  mentre il menu è aperto.
- Nei modali, Tab e gli eventi degli editor restano nello scope attivo;
  la chiusura ripristina il focus precedente secondo il contratto di navigazione.

## Escape

Escape viene consumato una sola volta per frame. Un menu chiude prima il
sottomenu più interno. Un dropdown aperto in un modale riceve Escape prima
del modale. Un editor attivo può annullare la modifica prima che Escape
chiuda il dialogo. La registrazione del consumo e le rivendicazioni del click
sono condivise, così l'evento non prosegue dietro l'overlay dopo la chiusura.

La cattura della rotella considera drag degli slider, dropdown e menu aperti
e il rettangolo del modale attivo. Un campo testo focalizzato da solo non
blocca lo scroll di un pannello.

## Confini privati e verifiche

`ui_capture_internal.h` dichiara la politica condivisa; `ui_overlay_internal.h`
fornisce i collegamenti del ciclo di vita usati dal frame e dalle transizioni.
Non sono header pubblici installati. Il modulo di cattura non disegna; il
controllo architetturale impedisce scritture delle rivendicazioni del click e
del consumo Escape dai moduli dei widget.

`ui_overlay_modules_test` collega normalmente la libreria e usa solo API
pubbliche. Verifica blocco della digitazione sotto menu, click di chiusura,
proprietà del dropdown sovrapposto, Escape tra dropdown/modale e tra livelli
di sottomenu, consumo degli eventi e ripristino del focus. È incluso in
Make, CTest, distribuzione SDK e nel target ASan/UBSan
`build/ui_overlay_modules_test_sanitize`.
