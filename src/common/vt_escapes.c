#include "common/vt_escapes.h"

#include <stdio.h>

TermSize query_term_size(void* arg, getc_f getc, puts_f puts) {
    // set the cursor to some arbitrarily large row/col, such that the response
    // returns the actual dimensions of the terminal.
    puts(arg, VT_SAVE VT_ROWCOL(999, 999) VT_GETPOS VT_RESTORE);

    // read VT_GETPOS response
    char vt_response[32];

    char c = '\0';
    unsigned int i = 0;
    while (c != 'R') {
        c = getc(arg);
        vt_response[i++] = c;
    }

    // extract dimensions from the response
    TermSize ret;
    int matches = sscanf(vt_response, "\033[%u;%uR", &ret.height, &ret.width);
    ret.success = matches == 2;
    return ret;
}
