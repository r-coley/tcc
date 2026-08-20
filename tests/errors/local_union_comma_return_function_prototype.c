union U { int x; };

void g(void) {
    union U obj, bad()(void);
}
