int
main(void)
{
	int value = 0;
	const int *cp = &value;
	volatile int *vp = &value;
	const volatile int *merged = 1 ? vp : cp;

	return merged ? 42 : 1;
}
