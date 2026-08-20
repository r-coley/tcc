int
takes_int_ptr_ptr(int **pp)
{
	return pp != 0;
}

int
main(void)
{
	char *cp = 0;
	char **cpp = &cp;

	return takes_int_ptr_ptr(cpp);
}
