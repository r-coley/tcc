static int a = 10;
static int b = 11;
static int *values[2] = { &a, &b };

int other_values_sum(void);

static int
self_values_sum(void)
{
	return *values[0] + *values[1];
}

int
main(void)
{
	return self_values_sum() + other_values_sum();
}
