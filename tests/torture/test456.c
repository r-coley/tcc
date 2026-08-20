/*
 * Test: (*ptr)[n] — indexing a dereferenced pointer-to-pointer.
 *
 * This form is valid C.  It previously caused a stage1 self-hosting failure
 * because new_deref() did not set is_pointer=1 when dereferencing T**, so
 * parse_postfix fell through to the fatal "not an array or pointer" error
 * when the Type* chain was not propagated correctly through self-compilation.
 *
 * Covers:
 *   - basic (*pp)[n] with char **
 *   - const char ** parameter (the preprocess.c pattern)
 *   - (*pp)[n] inside a compound && condition (short-circuit context)
 *   - (*pp)[n] used as a call argument
 */

/* Basic: char ** local */
static int test_basic(void) {
    char buf[4];
    buf[0] = 'a'; buf[1] = 'b'; buf[2] = 'c'; buf[3] = 0;
    char *p = buf;
    char **pp = &p;
    /* (*pp)[0] == 'a', (*pp)[1] == 'b', (*pp)[2] == 'c' */
    if ((*pp)[0] != 'a') return 1;
    if ((*pp)[1] != 'b') return 2;
    if ((*pp)[2] != 'c') return 3;
    return 0;
}

/* const char ** parameter — matches the preprocess.c pattern exactly */
static int starts_with_defined(const char **expr) {
    /* strncmp equivalent inlined to avoid header dependency */
    const char *kw = "defined";
    int i;
    for (i = 0; i < 7; i++) {
        if ((*expr)[i] != kw[i]) return 0;
    }
    /* (*expr)[7] must not be alphanumeric or '_' */
    char c = (*expr)[7];
    if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9') || c == '_')
        return 0;
    return 1;
}

static int test_const_param(void) {
    const char *s1 = "defined ";   /* trailing space — should match */
    const char *s2 = "defined_x";  /* trailing _ — should not match */
    const char *s3 = "definedX";   /* trailing letter — should not match */
    const char *s4 = "define";     /* too short — should not match */

    if (!starts_with_defined(&s1)) return 4;
    if ( starts_with_defined(&s2)) return 5;
    if ( starts_with_defined(&s3)) return 6;
    if ( starts_with_defined(&s4)) return 7;
    return 0;
}

/* (*pp)[n] inside compound && — tests the short-circuit IR path */
static int test_compound_condition(const char **expr) {
    /* Mirror the exact preprocess.c pattern */
    char c7 = (*expr)[7];
    int alnum = (c7 >= 'a' && c7 <= 'z') || (c7 >= 'A' && c7 <= 'Z') || (c7 >= '0' && c7 <= '9');
    if ((*expr)[0] == 'd' && (*expr)[1] == 'e' && !(alnum || (*expr)[7] == '_'))
        return 1;
    return 0;
}

static int test_condition(void) {
    const char *s = "defined (x)";
    if (!test_compound_condition(&s)) return 8;
    const char *s2 = "defined_x";
    if (test_compound_condition(&s2)) return 9;
    return 0;
}

/* (*pp)[n] as a function call argument */
static int identity(int c) { return c; }

static int test_as_argument(void) {
    const char *s = "hello";
    const char **pp = &s;
    if (identity((*pp)[0]) != 'h') return 10;
    if (identity((*pp)[4]) != 'o') return 11;
    return 0;
}

int main(void) {
    int r;
    r = test_basic();        if (r) return r;
    r = test_const_param();  if (r) return r;
    r = test_condition();    if (r) return r;
    r = test_as_argument();  if (r) return r;
    return 0;
}
