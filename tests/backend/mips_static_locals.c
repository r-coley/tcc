static int counter = 40;

int bump(void) {
    counter = counter + 1;
    return counter;
}

int main(void) {
    if (bump() != 41)
        return 1;
    if (bump() != 42)
        return 2;
    return 0;
}
