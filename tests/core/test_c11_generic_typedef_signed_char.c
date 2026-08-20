typedef signed char schar;

int f(schar c)
{
	return c;
}

int main(void)
{
	schar c = 0;
	schar *p = &c;
	int total = 0;

	total += _Generic(c, char: 1, signed char: 10, unsigned char: 2, default: 100);
	total += _Generic((schar)0, char: 1, signed char: 20, unsigned char: 2, default: 100);
	total += _Generic(p, char *: 1, signed char *: 30, unsigned char *: 2, default: 100);
	total += _Generic(f, int (*)(char): 1, int (*)(signed char): 40, default: 100);

	return total == 100 ? 42 : total;
}
