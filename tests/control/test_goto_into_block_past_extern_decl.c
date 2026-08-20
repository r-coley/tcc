int
main(void)
{
	goto inside;
	{
		extern int ext;
inside:
		return 42;
	}
}
