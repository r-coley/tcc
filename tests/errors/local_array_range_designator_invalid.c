int
main(void)
{
	int values[3] = { [2 ... 1] = 42 };

	return values[1];
}
