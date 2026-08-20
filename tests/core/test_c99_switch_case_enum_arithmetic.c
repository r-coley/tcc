enum E {
	E_A = 1,
	E_B = 2,
	E_C = 4
};

int
main(void)
{
	enum E e = E_C;

	switch (e) {
	case E_A | E_B:
		return 1;
	case E_A + E_B + 1:
		return 42;
	default:
		return 0;
	}
}
