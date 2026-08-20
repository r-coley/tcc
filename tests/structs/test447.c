/* char[] struct field used as pointer argument must decay to address,
   not load a byte. Tests both arrow (->) and dot (.) member access. */
int strncpy(char *dst, const char *src, unsigned long n);
int strlen(const char *s);

typedef struct {
    char name[32];
    int value;
} Entry;

static Entry entries[4];

void set_name(Entry *e, const char *s) {
    strncpy(e->name, s, 31);   /* char[] via -> must be address, not byte */
}

int main() {
    set_name(&entries[0], "hello");
    set_name(&entries[1], "world");
    /* strlen(entries[1].name) via global array index must also be address */
    return strlen(entries[1].name);
}
