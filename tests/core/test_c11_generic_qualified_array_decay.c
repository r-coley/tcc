#define TYPEOF(x) _Generic((x), \
	volatile int *: 1, \
	const volatile int *: 2, \
	volatile int (*)[3]: 3, \
	const volatile int (*)[3]: 4, \
	default: 99)

int
main(void)
{
	volatile int va[3] = {1, 2, 3};
	const volatile int cva[3] = {4, 5, 6};

	if (TYPEOF(va) != 1)
		return 1;
	if (TYPEOF(cva) != 2)
		return 2;
	if (TYPEOF(&va) != 3)
		return 3;
	if (TYPEOF(&cva) != 4)
		return 4;

	return 42;
}
