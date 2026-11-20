#ifndef VILAOS_INPUT_PUMP_H
#define VILAOS_INPUT_PUMP_H

#include "input.h"

int input_pump_start(InputMux *m);
void input_pump_stop(InputMux *m);
int input_pump_pop(InputMux *m, InputEvent *out, int timeout_ms);
int input_dev_read(InputMux *m, InputEvent *out, int timeout_ms);

#endif
