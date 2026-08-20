struct Point { int x; int y; };

int main(void) {
    return ((struct Point){ .x = 1, .y = 2 }).x;
}
