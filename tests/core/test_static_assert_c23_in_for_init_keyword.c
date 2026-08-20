int
main(void)
{
	int ran = 0;

	for (static_assert(sizeof(int) == 4); ran == 0; ran = 1) {
	}

	return ran ? 42 : 1;
}
