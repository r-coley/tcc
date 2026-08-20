/* Regression: heap pointer values stored/reloaded through a char ** array. */

#include <stdlib.h>
#include <string.h>

static char *make_string(const char *s) {
    size_t n = strlen(s) + 1;
    char *p = malloc(n);

    if (!p)
        return 0;

    memcpy(p, s, n);
    return p;
}

int main(void) {
    char **arena;
    char *p0;
    char *p1;

    arena = calloc(4, sizeof(char *));
    if (!arena)
        return 1;

    p0 = make_string("alpha");
    p1 = make_string("beta");

    if (!p0 || !p1)
        return 2;

    arena[0] = p0;
    arena[1] = p1;

    if (arena[0] != p0)
        return 10;
    if (arena[1] != p1)
        return 11;

    if (strcmp(arena[0], "alpha") != 0)
        return 20;
    if (strcmp(arena[1], "beta") != 0)
        return 21;

    free(arena[0]);
    free(arena[1]);
    free(arena);

    return 42;
}
