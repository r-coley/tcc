union U {
	char a[3];
	int i;
} gu = { .a = {
	1 ? 'A' : 'B',
	_Generic((_Bool)0, _Bool: 'C', int: 'D', default: 'E'),
	'Z'
} };

int
main(void)
{
	if (gu.a[0] != 'A')
		return 1;
	if (gu.a[1] != 'C')
		return 2;
	if (gu.a[2] != 'Z')
		return 3;
	return 42;
}
