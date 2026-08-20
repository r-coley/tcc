int a[sizeof(sizeof(int)) == sizeof(unsigned long) ? 3 : -1];
int b[0 ? -1 : 4];
enum { ALIGN_BOUND = _Alignof(int) == 4 ? 5 : -1 };
int c[ALIGN_BOUND];

int main(void)
{
	if (sizeof(a) != sizeof(int) * 3)
		return 1;
	if (sizeof(b) != sizeof(int) * 4)
		return 2;
	if (sizeof(c) != sizeof(int) * 5)
		return 3;
	return 42;
}
