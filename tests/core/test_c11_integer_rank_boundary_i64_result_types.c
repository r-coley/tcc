#define TYPEOF(x) _Generic((x), \
	int: 1, \
	unsigned int: 2, \
	long: 3, \
	unsigned long: 4, \
	long long: 5, \
	unsigned long long: 6, \
	default: 99)

int
main(void)
{
	unsigned int ui = 1U;
	long long sll = -1LL;
	unsigned long long ull = 1ULL;

	if (sizeof(long long) != 8)
		return 100;

	if (TYPEOF(sll + ui) != 5)
		return 1;
	if (TYPEOF(ui + sll) != 5)
		return 2;
	if (TYPEOF(ui ? sll : ui) != 5)
		return 3;
	if (TYPEOF(ui ? ui : sll) != 5)
		return 4;

	if ((sll + ui) != 0LL)
		return 5;
	if ((ui + sll) != 0LL)
		return 6;
	if ((ui ? sll : ui) != -1LL)
		return 7;
	if ((0 ? ui : sll) != -1LL)
		return 8;

	if (TYPEOF(ull + sll) != 6)
		return 9;
	if ((ull + sll) != 0ULL)
		return 10;

	return 42;
}
