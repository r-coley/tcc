int duplicated_plain_inline_with_proto(void);

inline int
duplicated_plain_inline_with_proto(void)
{
	return 40;
}

int lib_plain_inline_value_with_proto(void);

int
main(void)
{
	return lib_plain_inline_value_with_proto();
}
