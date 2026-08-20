volatile int **
ret_nested(int **pp)
{
	return pp;
}

int
main(void)
{
	int value = 0;
	int *p = &value;
	int **pp = &p;

	return ret_nested(pp) != 0;
}
