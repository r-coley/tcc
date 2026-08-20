static short
ret_short(void)
{
	return 10;
}

static unsigned long
ret_ulong(void)
{
	return 20ul;
}

static long long
ret_ll(void)
{
	return 12ll;
}

int
main(void)
{
	short (*fp_short)(void) = ret_short;
	unsigned long (*fp_ulong)(void) = ret_ulong;
	long long (*fp_ll)(void) = ret_ll;

	return fp_short() + (int)fp_ulong() + (int)fp_ll();
}
