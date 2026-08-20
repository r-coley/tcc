int
main(void)
{
	int value = 0;
	void *vp = &value;
	const int *cp = &value;
	const void *merged = 1 ? cp : vp;

	return merged ? 0 : 1;
}
