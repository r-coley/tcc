typedef int I;
typedef unsigned short US;

int
main(void)
{
	I i = (I){ 40 };
	char c = (char){ 2 };
	US s = (US){ 0xffffu };

	if ((I){ 1 } + (I){ 2 } != 3)
		return 1;
	if (i + c != 42)
		return 2;
	if (s != 65535u)
		return 3;
	if ((I){{ 5 }} != 5)
		return 4;
	if ((US){{ 0xffffu, }, } != 65535u)
		return 5;
	return 42;
}
