int main(void)
{
	double d = 3.75;
	float f = (float)d;
	double back = (double)f;

	if (f != 3.75f)
		return 1;
	if (back != 3.75)
		return 2;
	if ((float)(d + 0.5) != 4.25f)
		return 3;
	return 42;
}
