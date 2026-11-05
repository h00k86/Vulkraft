#pragma once

#include <stdbool.h>
typedef struct APP APP;
#include "log/log.h"


// Dichiarazione della funzione principale per il disegno di un frame
bool wait_for_current_frame(APP *app);
bool draw_frame(APP *app);
