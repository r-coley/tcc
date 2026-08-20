typedef const int carr3[3];

int
main(void)
{
	const int a[3] = {0};
	return _Generic(&a, const int (*)[3]: 1, carr3 *: 2, default: 3);
}
