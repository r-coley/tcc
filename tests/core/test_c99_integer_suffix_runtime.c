int
main(void)
{
	unsigned long ul = 42UL;
	unsigned long long ull = 42ULL;

	if (42Lu != ul)
		return 1;
	if (42lU != ul)
		return 2;
	if (42Ul != ul)
		return 3;
	if (42uL != ul)
		return 4;

	if (42LLu != ull)
		return 5;
	if (42llU != ull)
		return 6;
	if (42Ull != ull)
		return 7;
	if (42uLL != ull)
		return 8;

	return 42;
}
