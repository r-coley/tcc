int
takes_void_ptr(void *p)
{
	return p != 0;
}

int
main(void)
{
	const int *cp = 0;
	return takes_void_ptr(cp);
}
