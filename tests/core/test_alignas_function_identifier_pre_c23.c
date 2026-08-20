static int alignas(void)
{
	return 42;
}

int
main(void)
{
	return alignas();
}
