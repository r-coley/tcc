union U {
	_Alignas(16) int x;
	char c;
};

int
main(void)
{
	union U value;
	unsigned long offset = (unsigned long)((char *)&value.x - (char *)&value);

	if (_Alignof(union U) != 16)
		return 1;
	if (sizeof(union U) != 16)
		return 2;
	if (offset != 0UL)
		return 3;
	return 42;
}
