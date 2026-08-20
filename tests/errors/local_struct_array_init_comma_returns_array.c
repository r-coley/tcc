struct S { int x; };

void g(void) {
    struct S arr[1] = {{1}}, bad(void)[1];
}
