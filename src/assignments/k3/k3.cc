#include "common/ts7200.h"
#include "user/dbg.h"

#ifndef COMPILED_AT
#define COMPILED_AT "<unknown>"
#endif

void FirstUserTask() {
    printf("Compiled %s" ENDL, COMPILED_AT);
    printf("press any key to exit" ENDL);
    bwgetc(COM2);
}
