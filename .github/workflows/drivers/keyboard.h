// keyboard.h
#ifndef KEYBOARD_H
#define KEYBOARD_H
#include "../kernel/kernel.h"

void keyboard_init(void);
bool keyboard_has_key(void);
char keyboard_getchar(void);
char keyboard_try_getchar(void);
#endif
