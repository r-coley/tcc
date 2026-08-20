int
f(void)
{
	return 0;
}

int
main(void)
{
	void *vp;

	vp = f;
	return vp != 0;
}
