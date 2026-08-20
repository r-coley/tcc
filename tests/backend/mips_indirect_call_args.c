int sum3(int a, int b, int c) {
    return a + b + c;
}

int call_sum3(int (*fn)(int, int, int), int x) {
    return fn(x, x + 10, x + 20);
}

int main(void) {
    return call_sum3(sum3, 10);    /* 10 + 20 + 30 = 60 */
}
