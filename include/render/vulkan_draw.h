#pragma once

#include <stdlib.h>
typedef struct APP APP;
#include "log/log.h"


// Dichiarazione della funzione principale per il disegno di un frame
void wait_for_current_frame(APP *app);
void draw_frame(APP *app);
