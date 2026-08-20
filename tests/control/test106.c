int main() {
    int i;
    int j;
    int sum;

    i = 0;
    sum = 0;

    while (i < 3) {
        j = 0;

        while (j < 3) {
            j = j + 1;

            if (j == 2)
                continue;

            sum = sum + 1;
        }

        i = i + 1;
    }

    return sum;
}
