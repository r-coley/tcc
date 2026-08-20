int hits;

int bump(void) {
    hits = hits + 1;
    return hits;
}

int main(void) {
    int a;
    a = 0;
    if (a && bump())
        return 1;
    if (1 || bump())
        return hits;
    return 99;
}
