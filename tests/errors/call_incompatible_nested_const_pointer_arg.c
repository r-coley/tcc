int
takes_nested(const int **cpp)
{
	return cpp != 0;
}

int
main(void)
{
	int value = 0;
	int *p = &value;
	int **pp = &p;

	return takes_nested(pp);
}
