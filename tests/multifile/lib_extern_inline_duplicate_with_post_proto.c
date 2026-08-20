extern __inline __attribute__((__gnu_inline__))
int duplicated_header_inline_with_post_proto(void)
{
	return 40;
}

extern int duplicated_header_inline_with_post_proto(void);

int
lib_extern_inline_value_with_post_proto(void)
{
	return duplicated_header_inline_with_post_proto() + 2;
}
