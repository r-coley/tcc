int
f(void)
{
	return 0;
}

void
takes_void_ptr(void *p)
{
	(void)p;
}

int
main(void)
{
	takes_void_ptr(f);
	return 0;
}
