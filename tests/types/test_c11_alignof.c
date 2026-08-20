struct Mixed {
	char c;
	long l;
};

int main(void)
{
	int values[3];
	struct Mixed mixed;

	if (_Alignof(char) != 1)
		return 1;
	if (_Alignof(int) != 4)
		return 2;
	if (_Alignof(long) != 8)
		return 3;
	if (_Alignof(int *) != 8)
		return 4;
	if (_Alignof(values) != 4)
		return 5;
	if (_Alignof(mixed) != 8)
		return 6;
	if (_Alignof(struct Mixed) != 8)
		return 7;

	return 42;
}
