int main(void)
{
	double d = (double)3 + 0.5;
	float f = (float)2 + 0.25f;
	double from_float = (double)f;
	int a = (int)d;
	int b = (int)f;

	if (d != 3.5)
		return 1;
	if (f != 2.25f)
		return 2;
	if (from_float != 2.25)
		return 5;
	if (a != 3)
		return 3;
	if (b != 2)
		return 4;
	return 42;
}
