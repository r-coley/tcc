int
main(void)
{
	char *cp = 0;
	char **cpp = &cp;
	int **ipp = cpp;

	return ipp != 0;
}
