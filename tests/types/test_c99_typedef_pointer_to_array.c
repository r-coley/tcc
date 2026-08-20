int
main(void)
{
	int values[4] = {42, 0, 0, 0};
	typedef int (*IntArrayPtr)[4];
	IntArrayPtr ptr = &values;

	return (*ptr)[0];
}
