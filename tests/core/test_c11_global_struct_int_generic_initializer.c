struct S {
	int a;
	int b;
};

struct S gs = {
	_Generic((_Bool)0, _Bool: 11, int: 22, default: 33),
	1 ? 7 : 8
};

int
main(void)
{
	if (gs.a != 11)
		return 1;
	if (gs.b != 7)
		return 2;
	return 42;
}
