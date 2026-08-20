static void
side_effect(void)
{
}

int
main(void)
{
	1 ? side_effect() : 3;
	return 0;
}
