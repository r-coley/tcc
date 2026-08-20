int
sum_and_size(int n)
{
    int values[n];
    int i;
    int sum = 0;

    for (i = 0; i < n; i = i + 1) {
        values[i] = i + 1;
        sum = sum + values[i];
    }

    return sum + sizeof values + sizeof(values);
}

int
main(void)
{
    return sum_and_size(5) == 55 ? 42 : 0;
}
