enum E {
	E_A = 1,
	E_B = 2
};

int
main(void)
{
	int x = 2;

	switch (x) {
	case (enum E)E_A:
		return 1;
	case (enum E)E_B:
		return 42;
	default:
		return 2;
	}
}
