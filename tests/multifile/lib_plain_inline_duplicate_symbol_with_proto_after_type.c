int duplicated_plain_inline_with_proto_after_type(void);

int inline
duplicated_plain_inline_with_proto_after_type(void)
{
	return 40;
}

int
lib_plain_inline_value_with_proto_after_type(void)
{
	return duplicated_plain_inline_with_proto_after_type() + 2;
}
