int
main(void)
{
	int value = 0;
	const int *cp = &value;
	volatile int *vp = &value;
	int *bad = 1 ? cp : vp;

	return bad != 0;
}
