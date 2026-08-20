float ga[] = { 1.0f + 2.0f, (float)5 };

int
main(void)
{
	if (ga[0] != 3.0f)
		return 1;
	if (ga[1] != 5.0f)
		return 2;
	return 42;
}
