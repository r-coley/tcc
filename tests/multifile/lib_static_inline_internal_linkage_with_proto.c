static int local_inline_value_with_proto(void);

static inline int
local_inline_value_with_proto(void)
{
	return 19;
}

int
from_a_with_proto(void)
{
	return local_inline_value_with_proto();
}
