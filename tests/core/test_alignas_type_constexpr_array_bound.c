_Alignas(int[sizeof(int)]) int global_from_sizeof;
enum { ALIGN_COUNT = 8 };
_Alignas(int[ALIGN_COUNT]) int global_from_enum;

int
main(void)
{
	_Alignas(int[1 + 3]) int local_from_expr = 1;

	if (_Alignof(global_from_sizeof) != _Alignof(int))
		return 1;
	if (_Alignof(global_from_enum) != _Alignof(int))
		return 2;
	if (_Alignof(local_from_expr) != _Alignof(int))
		return 3;
	return 42;
}
