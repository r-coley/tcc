union U {
	int i;
	unsigned u;
} gu = { .i = _Generic((_Bool)0, _Bool: 11, int: 22, default: 33) };

int
main(void)
{
	return gu.i == 11 ? 42 : 1;
}
