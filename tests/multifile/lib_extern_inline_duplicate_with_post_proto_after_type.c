int extern __inline __attribute__((__gnu_inline__))
duplicated_header_inline_with_post_proto_after_type(void)
{
	return 40;
}

extern int duplicated_header_inline_with_post_proto_after_type(void);

int
lib_extern_inline_value_with_post_proto_after_type(void)
{
	return duplicated_header_inline_with_post_proto_after_type() + 2;
}
