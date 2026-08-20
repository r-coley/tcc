int
takes_void_ptr(void *p)
{
	return p != 0;
}

int
main(void)
{
	const volatile int *cvp = 0;
	return takes_void_ptr(cvp);
}
