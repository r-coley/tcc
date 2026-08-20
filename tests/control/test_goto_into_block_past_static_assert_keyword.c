int
main(void)
{
	goto inside;
	{
		static_assert(1, "ok");
inside:
		return 42;
	}
}
