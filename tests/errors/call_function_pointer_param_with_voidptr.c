int
f(void)
{
	return 0;
}

void
takes_fn(int (*fp)(void))
{
	(void)fp;
}

int
main(void)
{
	void *vp = 0;

	takes_fn(vp);
	return 0;
}
