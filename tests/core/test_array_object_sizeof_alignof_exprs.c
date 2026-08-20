int global_array[3];

int
main(void)
{
	int local_array[5];

	if (sizeof global_array != 3 * (int)sizeof(int))
		return 1;
	if (sizeof(global_array) != 3 * (int)sizeof(int))
		return 2;
	if (sizeof local_array != 5 * (int)sizeof(int))
		return 3;
	if (sizeof(local_array) != 5 * (int)sizeof(int))
		return 4;

#if __STDC_VERSION__ >= 201112L
	if (_Alignof(global_array) != _Alignof(int))
		return 5;
	if (_Alignof(local_array) != _Alignof(int))
		return 6;
#endif

	return 42;
}
