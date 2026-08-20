extern int add_one(int x), add_two(int x);
static int sub_one(int x), sub_two(int x);

int
add_one(int x)
{
	return x + 1;
}

int
add_two(int x)
{
	return x + 2;
}

static int
sub_one(int x)
{
	return x - 1;
}

static int
sub_two(int x)
{
	return x - 2;
}

int
main(void)
{
	return add_one(39) + add_two(1) + sub_one(4) + sub_two(3) - 5;
}
