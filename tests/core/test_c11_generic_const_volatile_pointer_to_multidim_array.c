#define TYPEOF(x) _Generic((x), \
	const volatile int (*)[2][2]: 4, \
	volatile int (*)[2][2]: 3, \
	const int (*)[2][2]: 2, \
	int (*)[2][2]: 1, \
	default: 0)

int
main(void)
{
	const volatile int m[2][2] = {{0}};
	return TYPEOF(&m) == 4 ? 42 : 1;
}
