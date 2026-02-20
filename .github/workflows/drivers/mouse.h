// mouse.h
#ifndef MOUSE_H
#define MOUSE_H
#include "../kernel/kernel.h"

void mouse_init(void);
void mouse_get_state(int32_t* x, int32_t* y, uint8_t* buttons);
#endif
