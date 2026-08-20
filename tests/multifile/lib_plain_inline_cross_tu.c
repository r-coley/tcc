inline int
shared_inline_value(void)
{
	return 40;
}

int
call_inline_value(void)
{
	return shared_inline_value();
}
