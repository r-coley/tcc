int
main(void)
{
	int value = 0;
	volatile int *vp = &value;
	int *bad = 1 ? vp : &value;

	return bad != 0;
}
