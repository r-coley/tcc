int read_hidden_values_sum_after_type(void);

int static a = 10;
int static b = 11;
int static *values[] = { &a, &b };

int
main(void)
{
	return *values[0] + *values[1] + read_hidden_values_sum_after_type();
}
