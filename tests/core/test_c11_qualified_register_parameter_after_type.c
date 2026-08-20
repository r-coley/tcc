static int
read_register_after_type(const int register x)
{
	return x;
}

int
main(void)
{
	return read_register_after_type(42);
}
