int classify(int x) {
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
    if (classify(1) != 10)
        return 1;
    if (classify(5) != 50)
        return 2;
    if (classify(7) != 99)
        return 3;
    return 0;
}
