extern int header_inline_value_with_proto(int x);

int
header_inline_value_with_proto(int x)
{
	return x + 1;
}

int
call_header_inline_from_lib_with_proto(void)
{
	return header_inline_value_with_proto(41);
}
