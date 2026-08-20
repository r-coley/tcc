int
main(void)
{
	_Bool b = 1;

	if (_Generic(b, _Bool: 1, int: 2, default: 99) != 1)
		return 1;
	if (_Generic((_Bool)0, _Bool: 1, int: 2, default: 99) != 1)
		return 2;
	if (_Generic(!b, _Bool: 1, int: 2, default: 99) != 2)
		return 3;
	if (_Generic(+b, _Bool: 1, int: 2, default: 99) != 2)
		return 4;

	return 42;
}
