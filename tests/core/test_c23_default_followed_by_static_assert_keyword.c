int
main(void)
{
	switch (0) {
	case 1:
		return 0;
	default:
		static_assert(1, "ok");
		return 42;
	}
}
