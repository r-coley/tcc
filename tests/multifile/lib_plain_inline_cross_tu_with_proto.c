int shared_inline_value_with_proto(void);

inline int
shared_inline_value_with_proto(void)
{
	return 40;
}

int
call_inline_value_with_proto(void)
{
	return shared_inline_value_with_proto();
}
