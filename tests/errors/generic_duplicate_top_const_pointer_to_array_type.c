typedef int arr3[3];

int
main(void)
{
	int a[3] = {0};
	return _Generic(&a, int (* const)[3]: 1, arr3 *: 2, default: 3);
}
