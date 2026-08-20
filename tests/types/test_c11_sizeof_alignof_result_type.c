int main(void)
{
	int n = 3;
	int x = 0;
	int a = _Generic(sizeof(int), unsigned long: 10, default: 1);
	int b = _Generic(sizeof x, unsigned long: 11, default: 2);
	int c = _Generic(sizeof(int[n]), unsigned long: 12, default: 3);
	int d = _Generic(_Alignof(int), unsigned long: 9, default: 4);

	if ((sizeof(int) - 8) < 0)
		return 5;
	if ((_Alignof(int) - 8) < 0)
		return 6;
	if (sizeof(sizeof(int)) != sizeof(unsigned long))
		return 7;

	return a + b + c + d;
}
