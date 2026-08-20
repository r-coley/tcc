#define TYPEOF(x) _Generic((x), \
	const int (*)[2][2]: 2, \
	int (*)[2][2]: 1, \
	default: 0)

int
main(void)
{
	const int m[2][2] = {{0}};
	return TYPEOF(&m) == 2 ? 42 : 1;
}
