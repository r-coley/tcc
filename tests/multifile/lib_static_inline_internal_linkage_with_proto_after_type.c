int static local_inline_value_with_proto_after_type(void);

int static inline
local_inline_value_with_proto_after_type(void)
{
	return 19;
}

int
from_a_with_proto_after_type(void)
{
	return local_inline_value_with_proto_after_type();
}
