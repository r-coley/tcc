int
main(void)
{
	int si;
	unsigned int ui;
	long long sll;
	unsigned long long ull;

	si = -1;
	ui = 1U;
	sll = -2LL;
	ull = 1ULL;

	if (!(si > ui))
		return 1;
	if ((ui + 41) != 42U)
		return 2;
	if (!(sll > ull))
		return 3;
	if ((ull + 41ULL) != 42ULL)
		return 4;

	return 42;
}
