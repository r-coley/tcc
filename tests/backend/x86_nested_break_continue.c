int main(void) {
    int i;
    int j;
    int sum;
    sum = 0;
    i = 0;
    while (i < 4) {
        i = i + 1;
        if (i == 2)
            continue;
        j = 0;
        while (j < 4) {
            j = j + 1;
            if (j == 3)
                break;
            sum = sum + i + j;
        }
    }
    return sum - 24;
}
