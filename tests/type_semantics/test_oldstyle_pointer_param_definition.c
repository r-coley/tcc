int
fetch(p)
int *p;
{
	return *p;
}

int
main(void)
{
	int value = 42;
	return fetch(&value) - 42;
}
