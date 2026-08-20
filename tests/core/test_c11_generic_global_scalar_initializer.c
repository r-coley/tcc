int gi = _Generic((_Bool)0, _Bool: 11, int: 22, default: 33);
int gj = _Generic(+(_Bool)0, _Bool: 11, int: 44, default: 33);
int gk = _Generic(1 ? (_Bool)0 : (_Bool)1, _Bool: 55, int: 66, default: 33);

int
main(void)
{
	if (gi != 11)
		return 1;
	if (gj != 44)
		return 2;
	if (gk != 66)
		return 3;
	return 42;
}
