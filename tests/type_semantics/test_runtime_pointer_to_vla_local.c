static int
sum_pointer_to_runtime_vla(int n)
{
	int values[3] = {40, 1, 1};
	int (*p)[n] = &values;

	return (*p)[0] + (*p)[1] + (*p)[2];
}

int
main(void)
{
	return sum_pointer_to_runtime_vla(3) - 42;
}
