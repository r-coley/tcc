static int
sum_register_after_type(int register x, int register y)
{
	return x + y;
}

int
main(void)
{
	return sum_register_after_type(20, 22);
}
