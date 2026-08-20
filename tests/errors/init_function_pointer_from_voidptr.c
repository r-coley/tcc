int
main(void)
{
	void *vp = 0;
	int (*fp)(void) = vp;

	return fp != 0;
}
