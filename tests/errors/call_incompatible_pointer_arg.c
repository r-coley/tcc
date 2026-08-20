int
takes_int_ptr(int *p)
{
	return p != 0;
}

int
main(void)
{
	char *cp = 0;
	return takes_int_ptr(cp);
}
