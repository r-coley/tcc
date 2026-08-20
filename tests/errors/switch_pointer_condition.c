int
main(void)
{
	int value = 0;
	int *ptr = &value;

	switch (ptr) {
	case 0:
		return 1;
	default:
		return 0;
	}
}
