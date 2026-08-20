int extern __inline __attribute__((__gnu_inline__))
duplicated_header_inline_after_type(void)
{
	return 40;
}

int lib_extern_inline_value_after_type(void);

int
main(void)
{
	return lib_extern_inline_value_after_type();
}
