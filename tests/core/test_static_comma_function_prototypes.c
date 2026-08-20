static int add_one(int x), add_two(int x);

static int
add_one(int x)
{
	return x + 1;
}

static int
add_two(int x)
{
	return x + 2;
}

int
main(void)
{
	return add_one(38) + add_two(1);
}
