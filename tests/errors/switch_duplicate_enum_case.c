enum E {
	A = 4,
	B = 4
};

int
main(void)
{
	enum E e = A;

	switch (e) {
	case A:
		return 1;
	case B:
		return 2;
	}
	return 0;
}
