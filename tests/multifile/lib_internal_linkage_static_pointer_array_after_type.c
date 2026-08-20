int static a = 9;
int static b = 12;
int static *values[] = { &a, &b };

int
read_hidden_values_sum_after_type(void)
{
	return *values[0] + *values[1];
}
