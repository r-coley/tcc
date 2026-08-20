static int
sink(void *p)
{
	return p != 0;
}

int
main(void)
{
	int value = 42;
	return sink(&value) ? 42 : 0;
}
