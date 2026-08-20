#ifndef FILE_MACRO_A_H
#define FILE_MACRO_A_H

#include "file_macro_b.h"

static int file_macro_a_ok(void) {
    return file_macro_contains(__FILE__, "file_macro_a.h") && file_macro_b_ok();
}

#endif
