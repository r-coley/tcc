static double
add_double(double a, double b)
{
	return a + b;
}

static float
add_float(float a, float b)
{
	return a + b;
}

int main(void)
{
	if (add_double(1.5, 2.5) != 4.0)
		return 1;
	if (add_float(1.0f, 2.0f) != 3.0f)
		return 2;
	return 42;
}
