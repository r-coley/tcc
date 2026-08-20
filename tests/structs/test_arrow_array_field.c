/* Regression: ptr->array_field must give address, not dereference as pointer.
 * This tests all access patterns: index, substr addr, strcmp. */
#include <string.h>

typedef struct { int x; char name[32]; int y; } Item;
static Item items[4];

Item *get(int i) { return &items[i]; }

int main(void) {
    strcpy(items[0].name, "hello");  items[0].x = 1;  items[0].y = 10;
    strcpy(items[1].name, "world");  items[1].x = 2;  items[1].y = 20;
    strcpy(items[2].name, "test");   items[2].x = 3;  items[2].y = 30;
    strcpy(items[3].name, "x");      items[3].x = 4;  items[3].y = 40;

    /* Direct index */
    if (get(0)->name[0] != 'h') return 1;
    if (get(3)->name[0] != 'x') return 2;  /* 'x' = 0x78; crash if treated as ptr */

    /* Explicit address-of */
    if (&get(0)->name[0] == (char *)0) return 3;

    /* strcmp */
    if (strcmp(get(0)->name, "hello") != 0) return 4;
    if (strcmp(get(1)->name, "world") != 0) return 5;
    if (strcmp(&get(2)->name[0], "test") != 0) return 6;

    /* Non-name field still works */
    if (get(2)->y != 30) return 7;

    return 42;
}
