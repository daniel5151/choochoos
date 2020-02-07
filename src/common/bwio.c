#pragma GCC diagnostic ignored "-Wconversion"
#pragma GCC diagnostic ignored "-Wsign-conversion"

/*
 * bwio.c - busy-wait I/O routines for diagnosis
 *
 * Specific to the TS-7200 ARM evaluation board
 *
 */

#include "common/bwio.h"
#include "common/printf.h"
#include "common/ts7200.h"

#include <ctype.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>

/*
 * The UARTs are initialized by RedBoot to the following state
 * 	115,200 bps
 * 	8 bits
 * 	no parity
 * 	fifos enabled
 */
int bwsetfifo(int channel, int state) {
    volatile int *line, buf;
    switch (channel) {
        case COM1:
            line = (volatile int*)(UART1_BASE + UART_LCRH_OFFSET);
            break;
        case COM2:
            line = (volatile int*)(UART2_BASE + UART_LCRH_OFFSET);
            break;
        default:
            return -1;
            break;
    }
    buf = *line;
    buf = state ? buf | FEN_MASK : buf & ~FEN_MASK;
    *line = buf;
    return 0;
}

int bwsetspeed(int channel, int speed) {
    volatile int *high, *low;
    switch (channel) {
        case COM1:
            high = (volatile int*)(UART1_BASE + UART_LCRM_OFFSET);
            low = (volatile int*)(UART1_BASE + UART_LCRL_OFFSET);
            break;
        case COM2:
            high = (volatile int*)(UART2_BASE + UART_LCRM_OFFSET);
            low = (volatile int*)(UART2_BASE + UART_LCRL_OFFSET);
            break;
        default:
            return -1;
            break;
    }
    switch (speed) {
        case 115200:
            *high = 0x0;
            *low = 0x3;
            return 0;
        case 2400:
            *high = 0x0;
            *low = 0xbf;  // fix baud diff eq
            return 0;
        default:
            return -1;
    }
}

int bwputc(int channel, char c) {
    volatile int *flags, *data;
    switch (channel) {
        case COM1:
            flags = (volatile int*)(UART1_BASE + UART_FLAG_OFFSET);
            data = (volatile int*)(UART1_BASE + UART_DATA_OFFSET);
            break;
        case COM2:
            flags = (volatile int*)(UART2_BASE + UART_FLAG_OFFSET);
            data = (volatile int*)(UART2_BASE + UART_DATA_OFFSET);
            break;
        default:
            return -1;
            break;
    }
    while ((*flags & TXFF_MASK))
        ;
    *data = c;
    return 0;
}

int bwputstr(int channel, const char* str) {
    while (*str) {
        if (bwputc(channel, *str) < 0) return -1;
        str++;
    }
    return 0;
}

int bwgetc(int channel) {
    volatile int *flags, *data;
    unsigned char c;

    switch (channel) {
        case COM1:
            flags = (volatile int*)(UART1_BASE + UART_FLAG_OFFSET);
            data = (volatile int*)(UART1_BASE + UART_DATA_OFFSET);
            break;
        case COM2:
            flags = (volatile int*)(UART2_BASE + UART_FLAG_OFFSET);
            data = (volatile int*)(UART2_BASE + UART_DATA_OFFSET);
            break;
        default:
            return -1;
            break;
    }
    while (!(*flags & RXFF_MASK))
        ;
    c = *data;
    return c;
}

void bwputc_(char c, void* arg) {
    // arg cannot be null
    bwputc(*(int*)arg, c);
}

int bwprintf(int channel, const char* format, ...) {
    va_list va;

    va_start(va, format);
    const int n = vfctprintf(&bwputc_, &channel, format, va);
    va_end(va);

    return n;
}

//----------------------------- Custom Additions -----------------------------//

void bwgetline(char* line, size_t len) {
    size_t i = 0;
    while (true) {
        char c = (char)bwgetc(COM2);
        if (c == '\r') break;
        if (c == '\b') {
            if (i == 0) continue;
            i--;
            bwputstr(COM2, "\b \b");
            continue;
        }
        if (i == len - 1) {
            // out of space - output a "ding" sound and ask for another
            // character
            bwputc(COM2, '\a');
            continue;
        }
        if (!isprint(c)) continue;
        line[i++] = c;
        bwputc(COM2, c);
    }
    bwputstr(COM2, "\r\n");
    line[i] = '\0';
}
