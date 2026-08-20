extern __inline __attribute__((__gnu_inline__))
int duplicated_header_inline(void)
{
	return 40;
}

int lib_extern_inline_value(void)
{
	return duplicated_header_inline() + 2;
}
