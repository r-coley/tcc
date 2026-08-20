int
main(void)
{
	int value = 0;
	void *vp = &value;
	const volatile int *cvp = &value;
	const volatile void *merged = 1 ? cvp : vp;

	return merged ? 42 : 1;
}
