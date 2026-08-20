int inline
duplicated_plain_inline_after_type(void)
{
	return 40;
}

int from_a_after_type(void);

int
main(void)
{
	return from_a_after_type();
}
