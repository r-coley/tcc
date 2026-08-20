int ga[3] = {
	[1] = _Generic((_Bool)0, _Bool: 11, int: 22, default: 33),
	[2] = 1 ? 7 : 8
};

int
main(void)
{
	if (ga[0] != 0)
		return 1;
	if (ga[1] != 11)
		return 2;
	if (ga[2] != 7)
		return 3;
	return 42;
}
