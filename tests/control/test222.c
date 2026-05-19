int main() {
    int i = 0;
    int sum = 0;
    for (;;) {
        if (i == 5)
            break;
        sum = sum + i;
        i = i + 1;
    }
    return sum;
}
