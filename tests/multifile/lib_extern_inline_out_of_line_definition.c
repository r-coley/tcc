int
header_inline_value(int x)
{
	return x + 1;
}

int
call_header_inline_from_lib(void)
{
	return header_inline_value(41);
}
