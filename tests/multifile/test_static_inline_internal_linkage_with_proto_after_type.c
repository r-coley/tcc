int from_a_with_proto_after_type(void);

int static local_inline_value_with_proto_after_type(void);

int static inline
local_inline_value_with_proto_after_type(void)
{
	return 21;
}

int
main(void)
{
	return from_a_with_proto_after_type() +
	       local_inline_value_with_proto_after_type() + 2;
}
