int from_a_after_type(void);

int static inline
local_inline_value_after_type(void)
{
	return 21;
}

int
main(void)
{
	return from_a_after_type() + local_inline_value_after_type() + 2;
}
