static int take_float(float value)
{
	return value == 3.5f;
}

static int take_double(double value)
{
	return value == 4.25;
}

static float ret_float_from_double(double value)
{
	return value;
}

static double ret_double_from_float(float value)
{
	return value;
}

static float ret_float_from_int(int value)
{
	return value;
}

static int ret_int_from_float(float value)
{
	return value;
}

int main(void)
{
	float f = 3.5;
	double d = 4.25f;

	if (f != 3.5f)
		return 1;
	if (d != 4.25)
		return 2;
	if (!take_float(3.5))
		return 3;
	if (!take_double(4.25f))
		return 4;
	if (ret_float_from_double(5.5) != 5.5f)
		return 5;
	if (ret_double_from_float(6.5f) != 6.5)
		return 6;
	if (ret_float_from_int(7) != 7.0f)
		return 7;
	if (ret_int_from_float(8.75f) != 8)
		return 8;

	return 42;
}
