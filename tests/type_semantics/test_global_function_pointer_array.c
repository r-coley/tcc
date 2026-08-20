static int
id(int x)
{
	return x;
}

int (*fps[2])(int);

int
main(void)
{
	fps[0] = id;
	return fps[0](7) - 7;
}
