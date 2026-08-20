int
do_return(int n)
{
    int values[n];
    int i;
    int sum = 0;

    for (i = 0; i < n; i = i + 1) {
        values[i] = i + 1;
        sum = sum + values[i];
    }

    return sum;
}

int
main(void)
{
    return do_return(5) == 15 ? 42 : 1;
}
