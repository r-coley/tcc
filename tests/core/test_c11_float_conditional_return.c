double pick_double(int c, float f, double d)
{
	return c ? f : d;
}

float pick_float(int c, float f, double d)
{
	return c ? d : f;
}

double nested_double(int a, int b, float f, double d)
{
	return a ? (b ? f : d) : (b ? d : f);
}

int main(void)
{
	if (pick_double(1, 1.5f, 2.5) != 1.5)
		return 1;
	if (pick_double(0, 1.5f, 2.5) != 2.5)
		return 2;
	if (pick_float(1, 1.5f, 2.5) != 2.5f)
		return 3;
	if (pick_float(0, 1.5f, 2.5) != 1.5f)
		return 4;
	if (nested_double(1, 1, 3.5f, 4.5) != 3.5)
		return 5;
	if (nested_double(1, 0, 3.5f, 4.5) != 4.5)
		return 6;
	if (nested_double(0, 1, 3.5f, 4.5) != 4.5)
		return 7;
	if (nested_double(0, 0, 3.5f, 4.5) != 3.5)
		return 8;
	return 42;
}
