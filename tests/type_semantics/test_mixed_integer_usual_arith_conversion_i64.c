int
main(void)
{
	unsigned int ui = 4000000000U;
	long long sll_neg = -1LL;
	long long sll_big = 5000000000LL;
	unsigned long long one_ull = 1ULL;
	unsigned long long max_ull = 0xFFFFFFFFFFFFFFFFULL;
	long long neg_two_ll = -2LL;

	if (sizeof(long long) != 8)
		return 100;

	/* unsigned int converts to signed long long when long long can represent it. */
	if ((ui + sll_neg) != 3999999999LL)
		return 1;
	if ((ui < sll_neg) != 0)
		return 2;
	if ((ui + sll_big) != 9000000000LL)
		return 3;
	if ((ui < sll_big) != 1)
		return 4;

	/* unsigned long long and signed long long of equal rank compare in the unsigned domain. */
	if ((one_ull < neg_two_ll) != 1)
		return 5;
	if ((max_ull < neg_two_ll) != 0)
		return 6;
	if ((one_ull + neg_two_ll) != 0xFFFFFFFFFFFFFFFFULL)
		return 7;
	if ((max_ull + 1LL) != 0ULL)
		return 8;

	return 0;
}
