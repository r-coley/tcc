int
main(void)
{
	int a[4] = {40, 1, 1, 0};
	int (*const p)[4] = &a;
	return (*p)[0] + (*p)[1] + (*p)[2];
}
