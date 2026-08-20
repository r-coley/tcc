int hits;

int bump(void) {
    hits = hits + 1;
    return hits;
}

int main(void) {
    hits = 0;

    if (0 && bump())
        return 1;
    if (hits != 0)
        return 2;

    if (1 || bump()) {
        if (hits != 0)
            return 3;
    }

    if (bump() && bump()) {
        if (hits != 2)
            return 4;
    }

    return 0;
}
