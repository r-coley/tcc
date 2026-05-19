int main() {
    int i;
    int total;

    i = 0;
    total = 0;

    while (i < 5) {
        i = i + 1;

        if (i == 3)
            continue;

        total = total + i;
    }

    return total;
}
