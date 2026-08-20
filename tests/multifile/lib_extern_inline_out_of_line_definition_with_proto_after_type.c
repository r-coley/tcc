int extern header_inline_value_with_proto_after_type(int x);

int
header_inline_value_with_proto_after_type(int x)
{
	return x + 1;
}

int
call_header_inline_from_lib_with_proto_after_type(void)
{
	return header_inline_value_with_proto_after_type(41);
}
