extern __inline __attribute__((__gnu_inline__))
int header_inline_value(int x)
{
	return x + 1;
}

int call_header_inline_from_lib(void);

int
main(void)
{
	return call_header_inline_from_lib() == 42 ? 42 : 1;
}
