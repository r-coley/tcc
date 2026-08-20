static int counter;

static int
next_value(void)
{
	return counter++;
}

int
main(void)
{
	switch (next_value()) {
	case 1:
		return 1;
	case 0:
		return counter == 1 ? 42 : 2;
	default:
		return 3;
	}
}
