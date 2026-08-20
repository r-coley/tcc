int
main(void)
{
	int i = 1;
	int values[3] = { [i] = 42 };

	return values[1];
}
