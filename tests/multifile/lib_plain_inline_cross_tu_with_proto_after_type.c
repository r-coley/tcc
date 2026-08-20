int shared_inline_value_with_proto_after_type(void);

int inline
shared_inline_value_with_proto_after_type(void)
{
	return 40;
}

int
call_inline_value_with_proto_after_type(void)
{
	return shared_inline_value_with_proto_after_type();
}
