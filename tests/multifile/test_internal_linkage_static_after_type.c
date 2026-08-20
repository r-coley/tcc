int read_hidden_value_from_lib_after_type(void);

int static hidden_value_after_type = 20;

int
main(void)
{
	return hidden_value_after_type +
	       read_hidden_value_from_lib_after_type() + 2;
}
