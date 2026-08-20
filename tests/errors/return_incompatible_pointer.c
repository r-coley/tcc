int *
ret_int_ptr(void)
{
	char *cp = 0;
	return cp;
}

int
main(void)
{
	return ret_int_ptr() != 0;
}
