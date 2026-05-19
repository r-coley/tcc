int main() {
    int i;
    int total;

    i = 0;
    total = 0;

    do {
        i = i + 1;

        if (i == 2)
            continue;

        total = total + i;
    } while (i < 3);

    return total;
}
