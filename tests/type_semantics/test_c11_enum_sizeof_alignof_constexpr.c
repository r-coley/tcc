enum {
	SIZEOF_UNSIGNED_COMPARE = ((sizeof(int) - 8) < 0),
	ALIGNOF_UNSIGNED_COMPARE = ((_Alignof(int) - 8) < 0),
	NESTED_SIZEOF_IS_SIZE_T = sizeof(sizeof(int)) == sizeof(unsigned long),
	CASTED_ENUM_EXPR = (int)(1 + 2 * 3)
};

_Static_assert(SIZEOF_UNSIGNED_COMPARE == 0, "sizeof arithmetic is unsigned");
_Static_assert(ALIGNOF_UNSIGNED_COMPARE == 0, "alignof arithmetic is unsigned");
_Static_assert(NESTED_SIZEOF_IS_SIZE_T == 1, "sizeof result is size_t");
_Static_assert(CASTED_ENUM_EXPR == 7, "enum constant expressions use full parser");

int main(void)
{
	if (SIZEOF_UNSIGNED_COMPARE != 0)
		return 1;
	if (ALIGNOF_UNSIGNED_COMPARE != 0)
		return 2;
	if (NESTED_SIZEOF_IS_SIZE_T != 1)
		return 3;
	if (CASTED_ENUM_EXPR != 7)
		return 4;
	return 42;
}
