void *
ret_void_ptr(void)
{
	const volatile int *cvp = 0;

	return cvp;
}

int
main(void)
{
	return ret_void_ptr() != 0;
}
