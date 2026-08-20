int pick(int a, int b, int c) {
    return a ? b + 1 : c + 2;
}

int main(void) {
    return pick(0, 10, 40);
}
