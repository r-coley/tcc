static int
check_param_vla_bound_side_effect(int n, int values[++n])
{
	return !(n == 4 && values[n - 1] == 13);
}

int
main(void)
{
	int values[4];

	values[0] = 2;
	values[1] = 3;
	values[2] = 5;
	values[3] = 13;

	return check_param_vla_bound_side_effect(3, values);
}
