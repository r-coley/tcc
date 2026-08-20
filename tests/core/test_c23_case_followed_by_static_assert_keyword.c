int
main(void)
{
	switch (2) {
	case 2:
		static_assert(1, "ok");
		return 42;
	default:
		return 0;
	}
}
