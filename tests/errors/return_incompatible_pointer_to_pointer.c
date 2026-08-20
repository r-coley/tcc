int **
ret_int_ptr_ptr(void)
{
	char *cp = 0;
	char **cpp = &cp;

	return cpp;
}

int
main(void)
{
	return ret_int_ptr_ptr() != 0;
}
