int inline
duplicated_plain_inline_with_post_proto_after_type(void)
{
	return 40;
}

int duplicated_plain_inline_with_post_proto_after_type(void);

int lib_plain_inline_value_with_post_proto_after_type(void);

int
main(void)
{
	return lib_plain_inline_value_with_post_proto_after_type();
}
