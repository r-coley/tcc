float ga[3] = { [0] = 1.0f + 2.0f, [2] = (float)5 };

int
main(void)
{
	if (ga[0] != 3.0f)
		return 1;
	if (ga[1] != 0.0f)
		return 2;
	if (ga[2] != 5.0f)
		return 3;
	return 42;
}
