int from_a(void);

static inline int
local_inline_value(void)
{
	return 21;
}

int
main(void)
{
	return from_a() + local_inline_value() + 2;
}
