typedef int **ipp;

int
main(void)
{
	int x = 0;
	int *p = &x;
	int **pp = &p;
	return _Generic(pp, int **: 1, ipp: 2, default: 3);
}
