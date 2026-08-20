int read_hidden_values_sum_after_type(void);

static int
ten(void)
{
	return 10;
}

static int
eleven(void)
{
	return 11;
}

int static (*values[])(void) = { ten, eleven };

int
main(void)
{
	return values[0]() + values[1]() + read_hidden_values_sum_after_type();
}
