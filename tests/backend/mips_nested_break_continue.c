int main(void) {
    int i;
    int j;
    int sum;

    sum = 0;
    i = 0;

    while (i < 4) {
        i = i + 1;
        j = 0;

        while (j < 4) {
            j = j + 1;

            if (j == 2)
                continue;

            if (i == 3)
                break;

            sum = sum + i + j;
        }
    }

    if (sum != 20)
        return 1;

    return 0;
}
