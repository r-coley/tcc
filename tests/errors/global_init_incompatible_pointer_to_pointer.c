int *ip;
char *cp;
int **ipp = &cp;

int
main(void)
{
	return ipp != 0;
}
