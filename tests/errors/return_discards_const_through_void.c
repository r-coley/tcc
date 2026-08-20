void *
ret_void_ptr(void)
{
	const int *cp = 0;
	return cp;
}

int
main(void)
{
	return ret_void_ptr() != 0;
}
