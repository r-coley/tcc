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

int static (*values[])(void) = { nine, twelve };

int
read_hidden_values_sum_after_type(void)
{
	return values[0]() + values[1]();
}
