static int
nine(void)
{
	return 9;
}

static int
twelve(void)
{
	return 12;
}

static int (*values[2])(void) = { nine, twelve };

int
other_values_sum(void)
{
	return values[0]() + values[1]();
}
