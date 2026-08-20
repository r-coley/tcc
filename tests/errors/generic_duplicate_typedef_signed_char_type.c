typedef signed char schar;

int main(void)
{
	schar c = 0;
	return _Generic(c, schar: 1, signed char: 2, default: 3);
}
