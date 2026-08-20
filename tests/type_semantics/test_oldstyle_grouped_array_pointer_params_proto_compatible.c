int g_arr[1] = { 20 };
int g_val = 22;

int sum(int a[], int *b);

int
sum(a, b)
int a[], *b;
{
	return a[0] + *b;
}

int
main(void)
{
	return sum(g_arr, &g_val) - 42;
}
