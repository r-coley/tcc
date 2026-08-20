int
sum_pointer_to_array(int (*values)[4]);

int
sum_pointer_to_array(int (*values)[4])
{
	return (*values)[2];
}

int values[4];

int
main(void)
{
	values[0] = 1;
	values[1] = 2;
	values[2] = 7;
	values[3] = 4;

	return sum_pointer_to_array(&values) - 7;
}
