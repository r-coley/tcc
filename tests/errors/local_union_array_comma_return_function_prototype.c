union U { int x; };

void g(void) {
    union U arr[1], bad()(void);
}
