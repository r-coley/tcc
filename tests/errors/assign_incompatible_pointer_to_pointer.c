int
main(void)
{
	int *ip = 0;
	char *cp = 0;
	int **ipp = &ip;
	char **cpp = &cp;

	ipp = cpp;
	return 0;
}
