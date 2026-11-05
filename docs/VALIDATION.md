# Validazione di Vulkraft

Il riferimento per comandi, dipendenze, report e limiti è
[PROJECT_VALIDATION.md](PROJECT_VALIDATION.md).

- `make test`: controlli architetturali e test automatici senza input desktop.
- `make test-sanitize`: test con AddressSanitizer e UndefinedBehaviorSanitizer.
- `make validate`: verifica generale, pacchetti sorgente e consumer SDK esterni.
- `make validate-native`: aggiunge i workflow nativi nelle finestre di test.
- `make benchmark-ui`: benchmark CPU della UI e dell'editor.

I risultati vengono generati localmente in `evidence/`. Un test headless non
costituisce una verifica del rendering Vulkan o dell'interazione nativa.
Le opzioni native richiedono le dipendenze e l'ambiente desktop descritti nella
guida di validazione. Supporto e limiti dell'SDK sono documentati in
[GRAPHICS_APP_API.md](GRAPHICS_APP_API.md) e [GRAPHICS_APP_SDK.md](GRAPHICS_APP_SDK.md).
