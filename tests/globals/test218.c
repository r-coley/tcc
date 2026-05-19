/* Global array of integers indexed with elem_size=4 — baseline regression */
static int table[8];

void table_set(int i, int v) { table[i] = v; }
int  table_get(int i) { return table[i]; }

int main() {
    table_set(0, 21);
    table_set(1, 21);
    return table_get(0) + table_get(1);
}
