int ga[] = {
	_Generic((_Bool)0, _Bool: 11, int: 22, default: 33),
	1 ? 7 : 8
};

int
main(void)
{
	if (ga[0] != 11)
		return 1;
	if (ga[1] != 7)
		return 2;
	return 42;
}
