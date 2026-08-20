static _Complex double *
complex_ptr_identity(_Complex double *value)
{
	return value;
}

static int
complex_ptr_is_nonnull(_Complex double *value)
{
	return value != 0;
}

int
main(void)
{
	_Complex double value = 42.0;
	_Complex double *ptr = complex_ptr_identity(&value);
	int ok = complex_ptr_is_nonnull(ptr);

	if (ptr != &value)
		return 1;
	if (!ok)
		return 2;
	return 42;
}
