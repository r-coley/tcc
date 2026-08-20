int main(void)
{
	float f = 1.25f;
	double d = 2.5;
	double sum = f + d;
	double diff = d - f;
	double prod = f * d;
	double quot = d / f;

	if (sum != 3.75)
		return 1;
	if (diff != 1.25)
		return 2;
	if (prod != 3.125)
		return 3;
	if (quot != 2.0)
		return 4;
	return 42;
}
