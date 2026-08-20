extern int duplicated_header_inline_with_proto(void);

extern __inline __attribute__((__gnu_inline__))
int duplicated_header_inline_with_proto(void)
{
	return 40;
}

int lib_extern_inline_value_with_proto(void);

int
main(void)
{
	return lib_extern_inline_value_with_proto();
}
