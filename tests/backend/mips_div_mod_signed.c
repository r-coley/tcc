int divs(int a, int b) {
    return a / b;
}

int mods(int a, int b) {
    return a % b;
}

int main(void) {
    if (divs(-21, 5) != -4)
        return 1;
    if (mods(-21, 5) != -1)
        return 2;
    return 0;
}
