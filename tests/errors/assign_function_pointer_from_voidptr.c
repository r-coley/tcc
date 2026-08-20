int
main(void)
{
	void *vp = 0;
	int (*fp)(void);

	fp = vp;
	return fp != 0;
}
