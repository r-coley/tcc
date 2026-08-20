static inline int
local_inline_value(void)
{
	return 19;
}

int
from_a(void)
{
	return local_inline_value();
}
