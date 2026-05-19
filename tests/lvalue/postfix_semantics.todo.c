/*
 * Enable when postfix value semantics are implemented.
 *
 * Expected: 12
 */
int main() {
    int x;
    int y;

    x = 1;
    y = x++;

    return y * 10 + x;
}
