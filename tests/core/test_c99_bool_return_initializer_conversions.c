static _Bool
ptr_to_bool(int *ptr)
{
	return ptr;
}

static _Bool
float_to_bool(float value)
{
	return value;
}

int
main(void)
{
	int value = 0;
	int *ptr = &value;
	_Bool from_ptr = ptr;
	_Bool from_null = (int *)0;

	if (from_ptr != 1)
		return 1;
	if (from_null != 0)
		return 2;
	if (ptr_to_bool(ptr) != 1)
		return 3;
	if (ptr_to_bool((int *)0) != 0)
		return 4;
	if (float_to_bool(0.5f) != 1)
		return 5;
	if (float_to_bool(0.0f) != 0)
		return 6;
	return 42;
}
