int from_a_with_proto(void);

static int local_inline_value_with_proto(void);

static inline int
local_inline_value_with_proto(void)
{
	return 21;
}

int
main(void)
{
	return from_a_with_proto() + local_inline_value_with_proto() + 2;
}
