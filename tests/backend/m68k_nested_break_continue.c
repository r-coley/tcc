int main(void) {
    int i;
    int j;
    int sum;

    sum = 0;

    for (i = 0; i < 4; i = i + 1) {
        for (j = 0; j < 4; j = j + 1) {
            if (j == 2)
                continue;

            if (i == 3)
                break;

            sum = sum + 1;
        }
    }

    if (sum != 9)
        return 1;

    return 0;
}
