union U {
	int i;
	unsigned u;
};

int
main(void)
{
	union U lu = { .i = _Generic((_Bool)0, _Bool: 11, int: 22, default: 33) };

	return lu.i == 11 ? 42 : 1;
}
