struct S {
	char c;
	long l;
};

int
main(void)
{
	const struct S cs;
	volatile struct S vs;

	if (_Alignof(struct S) != _Alignof(long))
		return 1;
	if (_Alignof(const struct S) != _Alignof(struct S))
		return 2;
	if (_Alignof(volatile struct S) != _Alignof(struct S))
		return 3;
	if (_Alignof(cs) != _Alignof(struct S))
		return 4;
	if (_Alignof(vs) != _Alignof(struct S))
		return 5;

	return 42;
}
