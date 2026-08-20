int
sum(a, b)
int *a, *b;
{
	return *a + *b;
}

int
main(void)
{
	int x = 20;
	int y = 22;
	return sum(&x, &y) - 42;
}
