int
main(void)
{
	int *ip = 0;
	char *cp = 0;
	return (1 ? ip : cp) != 0;
}
