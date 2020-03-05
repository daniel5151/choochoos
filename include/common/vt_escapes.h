// clang-format off
#pragma once

#define VT_ESC     "\033["

#define VT_RESET   "\033c"
#define VT_SAVE    "\0337"
#define VT_RESTORE "\0338"
#define VT_GETPOS  "\033[6n" // terminal responds with "\033<row>;<col>R"

#define VT_CLEAR    VT_ESC "2J"
#define VT_CLEARLN  VT_ESC "2K"
#define VT_CLEARLNR VT_ESC "K"

#define VT_TOPLEFT VT_ESC "H"
#define VT_ROWCOL(row, col) VT_ESC #row ";" #col "H"
#define VT_ROWCOL_FMT VT_ESC "%u;%uH"

#define VT_SET_SCROLL(startrow, endrow) VT_ESC #startrow ";" #endrow "r"
#define VT_SET_SCROLL_FMT VT_ESC "%u;%ur"

#define VT_UP(n)    VT_ESC #n "A"
#define VT_DOWN(n)  VT_ESC #n "B"
#define VT_RIGHT(n) VT_ESC #n "C"
#define VT_LEFT(n)  VT_ESC #n "D"

#define VT_NOFMT   VT_ESC "0m"
#define VT_BLACK   VT_ESC "30m"
#define VT_RED     VT_ESC "31m"
#define VT_GREEN   VT_ESC "32m"
#define VT_YELLOW  VT_ESC "33m"
#define VT_BLUE    VT_ESC "34m"
#define VT_MAGENTA VT_ESC "35m"
#define VT_CYAN    VT_ESC "36m"
#define VT_WHITE   VT_ESC "37m"

#define VT_HIDECUR VT_ESC "?25l"
#define VT_SHOWCUR VT_ESC "?25h"

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>

typedef struct {
    bool success;
    unsigned int width;
    unsigned int height;
} TermSize;

typedef char (*getc_f)(void* arg);
typedef void (*puts_f)(void* arg, const char* s);

/// Use VT_GETPOS to query the terminal's size.
///
/// NOTE: whatever stream getc is backed by should be drained prior to calling
/// this method. i.e: the first byte getc returns should be the start of the
/// VT100 response.
TermSize query_term_size(void* arg, getc_f getc, puts_f puts);

#ifdef __cplusplus
}
#endif
