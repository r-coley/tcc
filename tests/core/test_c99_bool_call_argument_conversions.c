static int
takes_bool(_Bool value)
{
	return value ? 42 : 1;
}

int
main(void)
{
	int local = 0;
	int *ptr = &local;

	if (takes_bool(ptr) != 42)
		return 2;
	if (takes_bool((int *)0) != 1)
		return 3;
	if (takes_bool(0.25f) != 42)
		return 4;
	if (takes_bool(0.0f) != 1)
		return 5;
	if (takes_bool(-7) != 42)
		return 6;
	if (takes_bool(0) != 1)
		return 7;
	return 42;
}
