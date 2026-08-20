int
sum(a, b)
int a[], *b;
{
    return a[0] + *b;
}

int sum(int a[], int *b);

int
main(void)
{
    int arr[1] = { 20 };
    int v = 22;
    return sum(arr, &v) - 42;
}
