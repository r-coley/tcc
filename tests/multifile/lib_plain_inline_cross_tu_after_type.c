int inline
shared_inline_value_after_type(void)
{
	return 40;
}

int
call_inline_value_after_type(void)
{
	return shared_inline_value_after_type();
}
