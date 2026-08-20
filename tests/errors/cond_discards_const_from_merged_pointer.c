int
main(void)
{
	int value = 0;
	const int *cp = &value;
	int *p = &value;
	int *bad = 1 ? cp : p;

	return bad != 0;
}
