int f(int x) {
    switch (x) {
    case 1:
        return 10;
    case 2:
        return 20;
    case 5:
        return 50;
    default:
        return 99;
    }
}

int main(void) {
    return f(5);
}
