int
main(void)
{
	goto inside;
	{
		void _Noreturn helper(void);
inside:
		return 42;
	}
}
