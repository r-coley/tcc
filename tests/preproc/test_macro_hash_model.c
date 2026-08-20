/* tests/preproc/test_macro_hash_model.c
   Model test for the planned preprocessor macro hash index.

   The real preprocessor should keep the macro array as the source of truth and
   store macro indexes in hash entries, not Macro * pointers, so array realloc
   cannot leave stale hash pointers behind.
*/

#include <stdlib.h>
#include <string.h>

typedef struct Macro {
    char name[32];
    int value;
} Macro;

typedef struct MacroHashEntry {
    char name[32];
    int macro_index;
    struct MacroHashEntry *next;
} MacroHashEntry;

static Macro *macros;
static int macro_count;
static int macro_cap;

static MacroHashEntry *buckets[64];

static unsigned hash_name(const char *s) {
    unsigned h = 2166136261u;

    while (*s) {
        h ^= (unsigned char)*s;
        h *= 16777619u;
        s++;
    }

    return h;
}

static void free_hash(void) {
    int i;

    for (i = 0; i < 64; i++) {
        MacroHashEntry *e = buckets[i];

        while (e) {
            MacroHashEntry *next = e->next;
            free(e);
            e = next;
        }

        buckets[i] = 0;
    }
}

static int find_index(const char *name) {
    unsigned b = hash_name(name) & 63u;
    MacroHashEntry *e = buckets[b];

    while (e) {
        if (strcmp(e->name, name) == 0)
            return e->macro_index;
        e = e->next;
    }

    return -1;
}

static int insert_hash(const char *name, int macro_index) {
    unsigned b = hash_name(name) & 63u;
    MacroHashEntry *e = malloc(sizeof(MacroHashEntry));

    if (!e)
        return 0;

    strcpy(e->name, name);
    e->macro_index = macro_index;
    e->next = buckets[b];
    buckets[b] = e;
    return 1;
}

static int remove_hash(const char *name) {
    unsigned b = hash_name(name) & 63u;
    MacroHashEntry *e = buckets[b];
    MacroHashEntry *prev = 0;

    while (e) {
        if (strcmp(e->name, name) == 0) {
            if (prev)
                prev->next = e->next;
            else
                buckets[b] = e->next;
            free(e);
            return 1;
        }

        prev = e;
        e = e->next;
    }

    return 0;
}

static int rebuild_hash(void) {
    int i;

    free_hash();

    for (i = 0; i < macro_count; i++) {
        if (!insert_hash(macros[i].name, i))
            return 0;
    }

    return 1;
}

static int ensure_macro_cap(void) {
    Macro *new_macros;
    int new_cap;

    if (macro_count < macro_cap)
        return 1;

    new_cap = macro_cap ? macro_cap * 2 : 4;
    new_macros = realloc(macros, (size_t)new_cap * sizeof(Macro));
    if (!new_macros)
        return 0;

    macros = new_macros;
    macro_cap = new_cap;
    return 1;
}

static int define_macro(const char *name, int value) {
    int idx = find_index(name);

    if (idx >= 0) {
        macros[idx].value = value;
        return 1;
    }

    if (!ensure_macro_cap())
        return 0;

    idx = macro_count++;
    strcpy(macros[idx].name, name);
    macros[idx].value = value;

    return insert_hash(name, idx);
}

static int undef_macro(const char *name) {
    int idx = find_index(name);
    int i;

    if (idx < 0)
        return 0;

    remove_hash(name);

    for (i = idx + 1; i < macro_count; i++)
        macros[i - 1] = macros[i];

    macro_count--;

    return rebuild_hash();
}

static int get_macro_value(const char *name, int *out) {
    int idx = find_index(name);

    if (idx < 0)
        return 0;

    *out = macros[idx].value;
    return 1;
}

int main(void) {
    int i;
    int value;
    char name[32];

    if (!define_macro("A", 10))
        return 1;
    if (!get_macro_value("A", &value) || value != 10)
        return 2;

    if (!define_macro("A", 42))
        return 3;
    if (!get_macro_value("A", &value) || value != 42)
        return 4;

    for (i = 0; i < 200; i++) {
        name[0] = 'M';
        name[1] = (char)('0' + ((i / 100) % 10));
        name[2] = (char)('0' + ((i / 10) % 10));
        name[3] = (char)('0' + (i % 10));
        name[4] = '\0';

        if (!define_macro(name, i))
            return 5;
    }

    if (!get_macro_value("M042", &value) || value != 42)
        return 6;
    if (!get_macro_value("M199", &value) || value != 199)
        return 7;

    if (!undef_macro("M042"))
        return 8;
    if (get_macro_value("M042", &value))
        return 9;

    if (!define_macro("M042", 42))
        return 10;
    if (!get_macro_value("M042", &value) || value != 42)
        return 11;

    if (!undef_macro("A"))
        return 12;
    if (get_macro_value("A", &value))
        return 13;

    free_hash();
    free(macros);

    return 42;
}
