#define TYPEOF(x) _Generic((x), \
	volatile int (*)[3]: 1, \
	const volatile int (*)[3]: 2, \
	volatile int (*)[2][3]: 3, \
	const volatile int (*)[2][3]: 4, \
	volatile int *: 5, \
	const volatile int *: 6, \
	default: 99)

int
main(void)
{
	volatile int vm[2][3] = {{0}};
	const volatile int cvm[2][3] = {{0}};

	if (TYPEOF(vm) != 1)
		return 1;
	if (TYPEOF(&vm) != 3)
		return 2;
	if (TYPEOF(vm[0]) != 5)
		return 3;

	if (TYPEOF(cvm) != 2)
		return 4;
	if (TYPEOF(&cvm) != 4)
		return 5;
	if (TYPEOF(cvm[0]) != 6)
		return 6;

	return 42;
}
