int bump(void) {
    static int counter = 40;
    counter = counter + 1;
    return counter;
}

int main(void) {
    return bump() + bump() - 41;
}
