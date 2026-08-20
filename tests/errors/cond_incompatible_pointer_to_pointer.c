int
main(void)
{
	int **ipp = 0;
	char **cpp = 0;
	return (1 ? ipp : cpp) != 0;
}
