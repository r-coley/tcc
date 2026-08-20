union U {
	char c;
	long l;
};

int
main(void)
{
	const union U cu;
	volatile union U vu;

	if (_Alignof(union U) != _Alignof(long))
		return 1;
	if (_Alignof(const union U) != _Alignof(union U))
		return 2;
	if (_Alignof(volatile union U) != _Alignof(union U))
		return 3;
	if (_Alignof(cu) != _Alignof(union U))
		return 4;
	if (_Alignof(vu) != _Alignof(union U))
		return 5;

	return 42;
}
