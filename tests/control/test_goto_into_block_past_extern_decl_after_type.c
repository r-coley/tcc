int
main(void)
{
	goto inside;
	{
		int extern ext;
inside:
		return 42;
	}
}
