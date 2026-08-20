static unsigned int
take_uint(unsigned int value)
{
	return value;
}

static unsigned int
ret_uint(long long value)
{
	return value;
}

int
main(void)
{
	unsigned int ui = -1LL;
	unsigned int from_cond = 1 ? -2LL : 7LL;
	unsigned int ui_assign = 0;

	if (ui != 4294967295U)
		return 1;
	if (from_cond != 4294967294U)
		return 2;

	ui_assign = -3LL;
	if (ui_assign != 4294967293U)
		return 3;

	if (take_uint(-4LL) != 4294967292U)
		return 4;
	if (ret_uint(-5LL) != 4294967291U)
		return 5;

	return 42;
}
