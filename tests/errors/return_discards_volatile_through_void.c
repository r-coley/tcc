void *
ret_void_ptr(void)
{
	volatile int *vp = 0;

	return vp;
}

int
main(void)
{
	return ret_void_ptr() != 0;
}
