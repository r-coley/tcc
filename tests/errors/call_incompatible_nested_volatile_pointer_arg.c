int
takes_nested(volatile int **vpp)
{
	return vpp != 0;
}

int
main(void)
{
	int value = 0;
	int *p = &value;
	int **pp = &p;

	return takes_nested(pp);
}
