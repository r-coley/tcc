float gf = 1.0f + 2.0f;
double gd = 1 ? 3.0 : 4.0;
float gh = (float)5;

int
main(void)
{
	if (gf != 3.0f)
		return 1;
	if (gd != 3.0)
		return 2;
	if (gh != 5.0f)
		return 3;
	return 42;
}
