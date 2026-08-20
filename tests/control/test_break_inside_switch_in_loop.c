int
main(void)
{
    int i;
    int sum = 0;

    for (i = 0; i < 5; i++) {
        switch (i) {
        case 1:
        case 3:
            break;
        default:
            sum += i;
            break;
        }
        sum += 10;
    }

    return (sum == 56) ? 42 : sum;
}
