/* Global struct array access to char[] field via [i].field indexing.
   macros[i].name pattern — must produce address not byte load. */
int strcmp(const char *a, const char *b);
int strncpy(char *dst, const char *src, unsigned long n);

typedef struct { char key[16]; int val; } KV;
static KV table[8];
static int table_len;

void kv_set(const char *k, int v) {
    strncpy(table[table_len].key, k, 15);
    table[table_len].val = v;
    table_len++;
}

int kv_get(const char *k) {
    for (int i = 0; i < table_len; i++)
        if (strcmp(table[i].key, k) == 0)
            return table[i].val;
    return -1;
}

int main() {
    kv_set("foo", 10);
    kv_set("bar", 32);
    return kv_get("foo") + kv_get("bar");
}
