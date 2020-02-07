#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/*
 * bwio.h
 */

#include <stddef.h>

#define COM1 0
#define COM2 1

#define ON 1
#define OFF 0

int bwsetfifo(int channel, int state);
int bwsetspeed(int channel, int speed);

int bwputc(int channel, char c);
int bwgetc(int channel);
int bwputstr(int channel, const char* str);

int bwprintf(int channel, const char* format, ...)
    __attribute__((format(printf, 2, 3)));

void bwgetline(char* line, size_t len);

#ifdef __cplusplus
}
#endif
