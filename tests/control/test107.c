int main() {
    int i;
    int sum;

    i = 0;
    sum = 0;

    do {
        i = i + 1;

        if (i == 2)
            continue;

        if (i == 5)
            break;

        sum = sum + i;
    } while (1);

    return sum;
}
