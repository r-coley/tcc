int
main(void)
{
    int i;
    int sum = 0;

    for (i = 0; i < 5; i++) {
        switch (i) {
        case 1:
        case 3:
            continue;
        default:
            sum += i;
            break;
        }
        sum += 10;
    }

    return (sum == 36) ? 42 : sum;
}
