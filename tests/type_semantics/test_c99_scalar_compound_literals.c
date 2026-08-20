int main(void)
{
	int i = (int){40};
	char c = (char){2};
	unsigned short s = (unsigned short){0xffffu};

	if ((int){1} + (int){2} != 3)
		return 1;
	if (i + c != 42)
		return 2;
	if (s != 65535u)
		return 3;
	if ((int){{5}} != 5)
		return 4;
	if ((unsigned short){{0xffffu, }, } != 65535u)
		return 5;
	return 42;
}
