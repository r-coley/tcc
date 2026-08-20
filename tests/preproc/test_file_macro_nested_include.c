/* Regression for __FILE__ while recursively preprocessing includes. */
#include "file_macro_a.h"

int main(void) {
    if (!file_macro_contains(__FILE__, "test_file_macro_nested_include.c"))
        return 1;
    if (!file_macro_a_ok())
        return 2;
    return 42;
}
