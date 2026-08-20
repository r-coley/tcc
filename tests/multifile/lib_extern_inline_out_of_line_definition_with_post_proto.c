int
header_inline_value_with_post_proto(int x)
{
	return x + 1;
}

extern int header_inline_value_with_post_proto(int x);

int
call_header_inline_from_lib_with_post_proto(void)
{
	return header_inline_value_with_post_proto(41);
}
