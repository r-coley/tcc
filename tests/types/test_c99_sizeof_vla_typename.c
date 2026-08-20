enum { ENUM_BOUND = 3 };

int main(void)
{
	int n = 2;
	int m = 3;
	int bytes = sizeof(int[++n][4]);

	if (n != 3 || bytes != 3 * 4 * (int)sizeof(int))
		return n;

	bytes = sizeof(int[++n][++m]);
	if (n != 4 || m != 4 || bytes != 4 * 4 * (int)sizeof(int))
		return n + m;

	if (sizeof(int[ENUM_BOUND]) != 3 * sizeof(int))
		return 3;
	if (sizeof(char[sizeof(int)]) != sizeof(int))
		return 4;

	return 42;
}
