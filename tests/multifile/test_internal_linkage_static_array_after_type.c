int read_hidden_values_sum_after_type(void);

int static hidden_values_after_type[2] = { 10, 11 };

int
main(void)
{
	return hidden_values_after_type[0] + hidden_values_after_type[1] +
	       read_hidden_values_sum_after_type();
}
