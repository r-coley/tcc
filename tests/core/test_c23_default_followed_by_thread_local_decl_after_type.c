int
main(void)
{
	switch (1) {
	case 0:
		return 0;
	default:
		int thread_local x = 1;
		return x;
	}
}
