static int a = 9;
static int b = 12;
static int *values[2] = { &a, &b };

int
other_values_sum(void)
{
	return *values[0] + *values[1];
}
