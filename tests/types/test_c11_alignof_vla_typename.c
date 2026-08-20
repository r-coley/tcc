enum { ENUM_BOUND = 3 };

int main(void)
{
	int n = 3;
	int a = _Alignof(int[n]);
	int b = _Alignof(int[++n]);

	if (n != 3 || a != _Alignof(int) || b != _Alignof(int))
		return n;
	if (_Alignof(int[ENUM_BOUND]) != _Alignof(int))
		return 1;
	if (_Alignof(char[sizeof(int)]) != _Alignof(char))
		return 2;

	return 42;
}
