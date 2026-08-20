int values[4];

int
(*get_values(void))[4]
{
	return &values;
}

int
main(void)
{
	(*get_values())[0] = 7;
	return (*get_values())[0] - 7;
}
