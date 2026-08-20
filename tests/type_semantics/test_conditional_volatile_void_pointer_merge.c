int
main(void)
{
	int value = 0;
	void *vp = &value;
	volatile int *vip = &value;
	volatile void *merged = 1 ? vp : vip;

	return merged ? 42 : 1;
}
