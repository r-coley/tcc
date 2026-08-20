int inline
duplicated_plain_inline_after_type(void)
{
	return 40;
}

int
from_a_after_type(void)
{
	return duplicated_plain_inline_after_type() + 2;
}
