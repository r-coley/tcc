static int values[2] = { 10, 11 };

int other_values_sum(void);

static int
self_values_sum(void)
{
	return values[0] + values[1];
}

int
main(void)
{
	return self_values_sum() + other_values_sum();
}
