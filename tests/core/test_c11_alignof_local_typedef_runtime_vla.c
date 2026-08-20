static int
f(int n)
{
	typedef int arr_t[n];
	arr_t a;
	arr_t *p = &a;

	if ((int)_Alignof(arr_t) != (int)_Alignof(int))
		return 1;
	if ((int)_Alignof(a) != (int)_Alignof(int))
		return 2;
	if ((int)_Alignof(*p) != (int)_Alignof(int))
		return 3;

	return 42;
}

int
main(void)
{
	return f(3);
}
