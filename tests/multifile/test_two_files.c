int multiply(int a, int b);
int add(int a, int b);
int main(void) {
    int x = multiply(6, 7);   /* 42 */
    int y = add(x, 0);        /* 42 */
    return y;
}
