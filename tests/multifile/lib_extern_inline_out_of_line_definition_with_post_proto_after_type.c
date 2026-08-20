int
header_inline_value_with_post_proto_after_type(int x)
{
	return x + 1;
}

int extern header_inline_value_with_post_proto_after_type(int x);

int
call_header_inline_from_lib_with_post_proto_after_type(void)
{
	return header_inline_value_with_post_proto_after_type(41);
}
