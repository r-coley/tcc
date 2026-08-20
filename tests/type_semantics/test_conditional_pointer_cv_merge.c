int
main(void)
{
	int value = 0;
	const int *cp = &value;
	volatile int *vp = &value;
	const volatile int *merged = 1 ? cp : vp;

	return merged ? 42 : 1;
}
