int pick(int flag, int a, int b) {
    return flag ? a : b;
}

int main(void) {
    if (pick(1, 42, 7) != 42)
        return 1;
    if (pick(0, 42, 7) != 7)
        return 2;
    return 0;
}
