_Alignas(8) long global_equal;
_Alignas(16) int global_stronger;

struct S {
	_Alignas(8) long field_equal;
	_Alignas(16) int field_stronger;
};

int
main(void)
{
	_Alignas(4) int local_equal = 3;
	_Alignas(16) int local_stronger = 4;

	if (_Alignof(global_equal) != 8)
		return 1;
	if (_Alignof(global_stronger) != 16)
		return 2;
	if (_Alignof(struct S) != 16)
		return 3;
	if (_Alignof(local_equal) != 4)
		return 4;
	if (_Alignof(local_stronger) != 16)
		return 5;

	return local_equal + local_stronger + 35;
}
