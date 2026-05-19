int main() {
    int i;
    int sum;

    i = 0;
    sum = 0;

    while (i < 5) {
        i = i + 1;

        if (i == 3)
            continue;

        sum = sum + i;
    }

    return sum;
}
