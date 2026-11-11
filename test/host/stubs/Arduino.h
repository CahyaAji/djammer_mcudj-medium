#ifndef ARDUINO_H
#define ARDUINO_H

#include <stdint.h>

#define LOW 0
#define HIGH 1
#define INPUT 0
#define OUTPUT 1
#define PA0 0
#define PA1 1
#define PA2 2
#define PA3 3
#define PA6 6
#define PA7 7
#define PB0 16
#define PB1 17
#define PB3 19
#define PB4 20
#define PB5 21
#define PB6 22
#define PB7 23
#define PB8 24
#define PB9 25
#define PC13 45
#define PC14 46

void digitalWrite(uint32_t pin, int value);
int digitalRead(uint32_t pin);
void pinMode(uint32_t pin, int mode);

#endif
