enum E {
	E_FOUR = 4
};

int
main(void)
{
	switch (E_FOUR) {
	case E_FOUR:
		return 1;
	case 4:
		return 2;
	}
	return 0;
}
