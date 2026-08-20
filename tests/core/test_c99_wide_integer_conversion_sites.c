static unsigned int
take_uint(unsigned int value)
{
	return value;
}

static long long
take_ll(long long value)
{
	return value;
}

static unsigned int
ret_uint(long long value)
{
	return value;
}

static long long
ret_ll(unsigned int value)
{
	return value;
}

int
main(void)
{
	unsigned int ui = -1LL;
	long long ll = 4000000000U;
	unsigned long long ull = -1LL;
	unsigned int from_cond = 1 ? -2LL : 7LL;
	long long ll_assign = 0;
	unsigned int ui_assign = 0;

	if (ui != 4294967295U)
		return 1;
	if (ll != 4000000000LL)
		return 2;
	if (ull != 18446744073709551615ULL)
		return 3;
	if (from_cond != 4294967294U)
		return 4;

	ll_assign = 1234567890U;
	ui_assign = -3LL;

	if (ll_assign != 1234567890LL)
		return 5;
	if (ui_assign != 4294967293U)
		return 6;

	if (take_uint(-4LL) != 4294967292U)
		return 7;
	if (take_ll(4000000000U) != 4000000000LL)
		return 8;
	if (ret_uint(-5LL) != 4294967291U)
		return 9;
	if (ret_ll(4000000001U) != 4000000001LL)
		return 10;

	return 42;
}
