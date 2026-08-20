#ifndef FILE_MACRO_B_H
#define FILE_MACRO_B_H

static int file_macro_contains(const char *s, const char *needle) {
    int i = 0;
    int j;

    if (!s || !needle)
        return 0;

    while (s[i]) {
        j = 0;
        while (needle[j] && s[i + j] == needle[j])
            j++;
        if (!needle[j])
            return 1;
        i++;
    }

    return 0;
}

static int file_macro_b_ok(void) {
    return file_macro_contains(__FILE__, "file_macro_b.h");
}

#endif
