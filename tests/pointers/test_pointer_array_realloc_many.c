/* Regression: growable char ** pointer table with many heap strings.
   This mirrors the old lexer token-text ownership table pattern. */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static char **items;
static int item_count;
static int item_cap;

static char *save_text(const char *s) {
    size_t n = strlen(s) + 1;
    char *copy = malloc(n);

    if (!copy)
        return 0;

    memcpy(copy, s, n);

    if (item_count >= item_cap) {
        int new_cap = item_cap ? item_cap * 2 : 8;
        char **new_items = realloc(items, (size_t)new_cap * sizeof(char *));

        if (!new_items) {
            free(copy);
            return 0;
        }

        items = new_items;
        item_cap = new_cap;
    }

    items[item_count++] = copy;
    return copy;
}

static void reset_texts(void) {
    int i;

    for (i = 0; i < item_count; i++)
        free(items[i]);

    free(items);
    items = 0;
    item_count = 0;
    item_cap = 0;
}

int main(void) {
    char *first;
    char *last;
    int i;

    first = save_text("first");
    if (!first)
        return 1;

    for (i = 0; i < 100; i++) {
        char buf[64];

        snprintf(buf, sizeof(buf), "item-%d", i);
        if (!save_text(buf))
            return 2;
    }

    last = save_text("last");
    if (!last)
        return 3;

    if (strcmp(first, "first") != 0)
        return 10;

    if (strcmp(last, "last") != 0)
        return 11;

    if (strcmp(items[0], "first") != 0)
        return 20;

    if (strcmp(items[item_count - 1], "last") != 0)
        return 21;

    reset_texts();

    return 42;
}
