int values[3] = { [2 ... 1] = 42 };

int
main(void)
{
	return values[1];
}
