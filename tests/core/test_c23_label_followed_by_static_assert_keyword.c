int
main(void)
{
	goto done;

done:
	static_assert(1, "ok");
	return 42;
}
