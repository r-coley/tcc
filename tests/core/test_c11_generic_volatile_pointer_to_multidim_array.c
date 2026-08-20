#define TYPEOF(x) _Generic((x), \
	volatile int (*)[2][2]: 3, \
	int (*)[2][2]: 1, \
	default: 0)

int
main(void)
{
	volatile int m[2][2] = {{0}};
	return TYPEOF(&m) == 3 ? 42 : 1;
}
