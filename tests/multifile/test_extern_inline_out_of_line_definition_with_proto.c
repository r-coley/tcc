extern int header_inline_value_with_proto(int x);

extern __inline __attribute__((__gnu_inline__))
int header_inline_value_with_proto(int x)
{
	return x + 1;
}

int call_header_inline_from_lib_with_proto(void);

int
main(void)
{
	return call_header_inline_from_lib_with_proto() == 42 ? 42 : 1;
}
