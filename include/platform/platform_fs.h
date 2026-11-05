#pragma once
/* File system portabile: contratti comuni per percorsi, file e directory.
 * I dettagli Windows/POSIX sono confinati nell'implementazione piattaforma.
 *
 * Convenzioni comuni a tutte le funzioni:
 *   - i percorsi si costruiscono e si passano SEMPRE con '/' come
 *     separatore; su Windows la conversione da '\' avviene qui;
 *   - nessuna funzione alloca: si scrive in un buffer del chiamante e si
 *     ritorna false se non ci sta (mai un troncamento silenzioso, che e' il
 *     modo piu' tipico di aprire il file sbagliato). */

#include <stdbool.h>
#include <stddef.h>

#define PLATFORM_PATH_MAX 1024
#define PLATFORM_NAME_MAX 256

/* --- interrogazione ---------------------------------------------------- */

/* Esiste, qualunque cosa sia (file, cartella, altro). */
bool platform_path_exists(const char *path);
/* Includes dangling symlinks/reparse points. */
bool platform_path_entry_exists(const char *path);
/* Canonical containment check. For a new file, only the final component may
 * be missing. Output is unchanged on failure. This is not an atomic open. */
bool platform_resolve_within(const char *root, const char *path,
                             bool allow_missing_leaf, char *out, size_t cap);
/* Esiste ed e' un file leggibile. */
bool platform_file_exists(const char *path);
/* Esiste ed e' una cartella. */
bool platform_dir_exists(const char *path);
/* Dimensione in byte; false se il file non esiste. */
bool platform_file_size(const char *path, long long *outBytes);
/* Ora dell'ultima modifica (secondi, confrontabile fra chiamate; il valore
 * assoluto non e' garantito uguale fra sistemi). false se non esiste. */
bool platform_file_mtime(const char *path, long long *outSeconds);

/* --- modifica ---------------------------------------------------------- */

/* Crea UNA cartella; true anche se esisteva gia'. */
bool platform_make_dir(const char *path);
/* Crea tutta la catena di cartelle del percorso. */
bool platform_make_dirs(const char *path);
bool platform_remove_file(const char *path);
/* Write a sibling temporary file, then replace path atomically. Failure leaves
 * the previous destination intact. Existing symlinks/non-regular files are
 * rejected. This is not conflict detection against other writers or a promise
 * of directory durability after power loss. */
bool platform_write_file_atomic(const char *path, const void *data, size_t size);
/* Rimuove una cartella VUOTA; false se non lo e'. */
bool platform_remove_dir(const char *path);

/* --- cartella di lavoro e percorsi ------------------------------------- */

bool platform_get_cwd(char *out, size_t cap);
bool platform_set_cwd(const char *path);
/* Percorso assoluto e normalizzato (i separatori escono sempre come '/').
 * false se il percorso non esiste. */
bool platform_real_path(const char *path, char *out, size_t cap);
/* Cartella dell'ESEGUIBILE in esecuzione: l'unico riferimento affidabile
 * per un'applicazione avviata da qualunque cartella. */
bool platform_exe_dir(char *out, size_t cap);

/* --- elenco di una cartella -------------------------------------------- */

typedef struct {
  char name[PLATFORM_NAME_MAX]; /* solo il nome, mai il percorso intero */
  bool isDir;
} PlatformDirEntry;

/* Opaco: la dimensione e' quella del piu' grande fra i due sistemi, cosi'
 * il chiamante puo' tenerlo sullo stack senza sapere cosa contiene. */
typedef struct {
  void *handle;
  char pattern[PLATFORM_PATH_MAX];
  unsigned char scratch[640];
  bool pending;
} PlatformDirIter;

/* Apre l'elenco. false se la cartella non esiste o non e' leggibile. */
bool platform_dir_open(PlatformDirIter *it, const char *path);
/* Voce successiva; false quando l'elenco e' finito. "." e ".." non vengono
 * MAI restituiti: nessun chiamante li voleva e ognuno li filtrava a modo
 * suo, che e' esattamente il genere di ripetizione che questo modulo
 * esiste per togliere. */
bool platform_dir_next(PlatformDirIter *it, PlatformDirEntry *out);
void platform_dir_close(PlatformDirIter *it);

/* Vero se la voce e' una cartella, con verifica reale quando l'elenco non
 * sapeva dirlo (dirPath = la cartella passata a platform_dir_open). */
bool platform_dir_entry_is_dir(const char *dirPath, const PlatformDirEntry *entry);

/* --- varie -------------------------------------------------------------- */

/* Riscrive in loco ogni '\\' come '/'. Va chiamata su qualunque percorso che
 * arrivi dall'esterno (drag&drop, riga di comando, file di progetto scritti
 * su Windows): nel framework i percorsi hanno una forma sola. */
void platform_fs_normalize_separators(char *path);

/* Imposta una variabile di ambiente con supporto Windows/POSIX. */
bool platform_setenv(const char *name, const char *value);
