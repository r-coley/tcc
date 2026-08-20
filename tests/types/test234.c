int add3(int x) {
    return x + 3;
}

int sub1(int x) {
    return x - 1;
}

int call_it(int (*fn)(int), int value) {
    return fn(value);
}

int choose_and_call(int use_add, int (*yes)(int), int (*no)(int), int value) {
    if (use_add)
        return yes(value);
    return no(value);
}

int main(void) {
    if (call_it(add3, 39) != 42)
        return 1;
    if (choose_and_call(0, add3, sub1, 43) != 42)
        return 2;
    if (choose_and_call(1, add3, sub1, 39) != 42)
        return 3;
    return 0;
}
