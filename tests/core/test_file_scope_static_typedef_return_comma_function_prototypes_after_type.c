typedef int myint;

myint static thirty_nine(void), three(void);

static myint
thirty_nine(void)
{
	return 39;
}

static myint
three(void)
{
	return 3;
}

int
main(void)
{
	return thirty_nine() + three();
}
