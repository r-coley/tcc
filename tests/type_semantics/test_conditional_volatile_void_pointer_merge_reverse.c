int
main(void)
{
	int value = 0;
	void *vp = &value;
	volatile int *vip = &value;
	volatile void *merged = 1 ? vip : vp;

	return merged ? 42 : 1;
}
