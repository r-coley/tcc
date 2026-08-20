int extern __inline __attribute__((__gnu_inline__))
header_inline_value_after_type(int x)
{
	return x + 1;
}

int call_header_inline_from_lib_after_type(void);

int
main(void)
{
	return call_header_inline_from_lib_after_type() == 42 ? 42 : 1;
}
