int
main(void)
{
	int value = 42;
	int *ip = &value;
	void *vp = &value;

	if (!(ip == vp))
		return 1;
	if (!(vp == ip))
		return 2;
	if (ip != vp)
		return 3;

	return 0;
}
