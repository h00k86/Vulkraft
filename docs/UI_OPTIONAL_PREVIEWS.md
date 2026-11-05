# Servizi anteprima opzionali

Il backend minimo richiede nove callback: rettangoli opachi e trasparenti,
testo, immagini, altezza e larghezza del testo, push/pop/profondità del clipping.
Le due decorazioni e i tre servizi immagine possono essere omessi.
Il disegno delle immagini resta obbligatorio; questo cambiamento riguarda le
query sulle dimensioni e il caricamento/rilascio dell'anteprima del picker.

## Configurazioni valide

| Dimensioni (`get_size`) | Caricamento (`preview_load`) | Rilascio (`preview_clear`) | Esito |
| --- | --- | --- | --- |
| Assente | Assente | Assente | Valido, servizio non supportato |
| Presente | Assente | Assente | Valido, query su immagini già gestite dal backend |
| Presente | Presente | Presente | Valido, servizio anteprima completo |

Tutte le altre combinazioni sono rifiutate dal collegamento del contesto e
dalla creazione della draw list. In particolare non si può registrare un loader
senza rilascio o senza query delle dimensioni. Le firme non cambiano e i backend
completi esistenti restano validi.

Senza callback, la query delle dimensioni restituisce false e lascia intatti
gli output; il caricamento restituisce -1; il rilascio non fa nulla. La draw list
mantiene lo stesso comportamento anche quando ha immagini accodate o già
riprodotte: un servizio assente non introduce un errore di risorsa né un rilascio
pendente. Quando il servizio esiste, restano i controlli che impediscono di
sostituire la preview mentre una geometria la può ancora referenziare.

Con `image_preview=true` e servizio assente, il picker mostra il messaggio
“Immagine non caricabile”. La selezione e la conferma del file funzionano ancora.
Il messaggio non distingue formato non supportato e backend senza loader.

## Verifiche

I test coprono tutte le otto combinazioni delle tre callback, query senza
modifica degli output, richieste non supportate prima/dopo il replay e il picker
con selezione/conferma senza servizio anteprima. I test preesistenti continuano
a controllare rilascio differito e conflitti con immagini accodate.
Lo standalone ora omette i servizi senza callback fittizie.

Evidenze: [evidence/ui-optional-previews](../evidence/ui-optional-previews/).

Risultati: 24 eseguibili di test, 21 anche con ASan/UBSan. Build debug, release
e headless senza warning. I nuovi percorsi sono verificati con renderer
simulati; runtime Vulkan e LeakSanitizer non ripetuti per questo intervento.
