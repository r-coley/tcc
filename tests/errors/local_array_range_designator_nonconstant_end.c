int
main(void)
{
	int hi = 2;
	int values[3] = { [0 ... hi] = 42 };

	return values[1];
}
