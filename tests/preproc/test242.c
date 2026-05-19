#define CALL(fn, ...) fn(0, ##__VA_ARGS__)

int value() {
    return CALL(foo);
}
