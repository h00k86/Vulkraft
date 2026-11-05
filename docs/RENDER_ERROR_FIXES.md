# Arresto ordinato dopo errori di rendering

Il percorso di rendering non usa più `exit(1)`. Gli errori fatali nelle attese,
nel reset e nella registrazione dei comandi, nell'upload, nell'acquisizione,
nel submit e nella presentazione vengono propagati tramite `APP.error`.
Il backend registra lo stage e il `VkResult`; il primo errore rimane disponibile
anche dopo lo shutdown.

`wait_for_current_frame`, `draw_frame` e `record_command_buffer` restituiscono
ora `bool`. Nei loop personalizzati, un risultato `false` richiede di fermare
il rendering e chiamare `app_shutdown`. `draw_frame == true` comprende anche
un frame saltato per ricreare la swapchain. Gli errori fatali non sono ritentati;
`OUT_OF_DATE` e `SUBOPTIMAL` mantengono invece il percorso di ricreazione.

Il loop standard interrompe le fasi successive al guasto e restituisce il
controllo al chiamante. L'esempio stampa l'errore, esegue lo shutdown e termina
con stato di fallimento. Il reset della fence grafica avviene solo dopo la
registrazione riuscita, subito prima del submit. Un errore di presentazione
ha precedenza su un resize pendente. Anche le attese durante la crescita del
vertex buffer e la ricreazione della swapchain propagano il fallimento.

## Verifica

- `make BUILD=debug` e `make BUILD=release`.
- `make test`: 14 eseguibili headless, incluso il controllo dei confini architetturali.
- `make test-sanitize`: 12 eseguibili con AddressSanitizer e UndefinedBehaviorSanitizer;
  LeakSanitizer disabilitato (`detect_leaks=0`).
- `render_failure_test`: 15 punti di guasto simulati, nessuna chiamata Vulkan
  successiva al guasto, blocco dei tentativi di rientro, casi di ricreazione.
- `app_loop_failure_test`: arresto dopo errore in poll, ricreazione, attesa,
  callback UI, preparazione e presentazione; shutdown da parte del chiamante.
- `init_failure_test`: cleanup ripetibile con `vkDeviceWaitIdle` simulato in
  `DEVICE_LOST`, conservazione dell'errore originale e rilascio delle risorse mock.

Log in [`evidence/render-error-fix`](../evidence/render-error-fix/).
Le prove di errore usano funzioni Vulkan simulate: non dimostrano il comportamento
di un driver con GPU realmente persa. Non sono state ripetute prove GUI o
validazioni su altri sistemi operativi. I precedenti risultati runtime rimangono
documentati separatamente in [RUNTIME_VALIDATION.md](RUNTIME_VALIDATION.md).
