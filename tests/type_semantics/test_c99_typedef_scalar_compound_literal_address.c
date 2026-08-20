typedef int I;
typedef unsigned short US;

int
main(void)
{
	I *p = &(I){ 42 };
	US *s = &(US){ 65535u };
	I *nested = &(I){{ 11 }};

	if (*p != 42)
		return 1;
	if (*s != 65535u)
		return 2;
	*p = 7;
	if (*p != 7)
		return 3;
	if (*nested != 11)
		return 4;
	*nested = 13;
	if (*nested != 13)
		return 5;
	return 42;
}
