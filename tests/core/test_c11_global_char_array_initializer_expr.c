char ga[] = {
	1 ? 'A' : 'B',
	_Generic((_Bool)0, _Bool: 'C', int: 'D', default: 'E')
};

int
main(void)
{
	if (ga[0] != 'A')
		return 1;
	if (ga[1] != 'C')
		return 2;
	return 42;
}
