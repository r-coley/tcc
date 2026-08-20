int
takes_void_ptr(void *p)
{
	return p != 0;
}

int
main(void)
{
	volatile int *vp = 0;

	return takes_void_ptr(vp);
}
