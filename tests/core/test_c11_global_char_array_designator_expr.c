char ga[3] = {
	[1] = 1 ? 'A' : 'B',
	[2] = _Generic((_Bool)0, _Bool: 'C', int: 'D', default: 'E')
};

int
main(void)
{
	if (ga[0] != 0)
		return 1;
	if (ga[1] != 'A')
		return 2;
	if (ga[2] != 'C')
		return 3;
	return 42;
}
