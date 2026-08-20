typedef int arr_t[3];

int
main(void)
{
	arr_t values;

	values[0] = 5;
	values[1] = 6;
	values[2] = 7;

	return values[0] - 5;
}
