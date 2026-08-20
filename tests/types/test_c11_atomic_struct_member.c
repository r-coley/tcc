struct AtomicPair {
	_Atomic int value;
	_Atomic(int *) ptr;
};

int
main(void)
{
	int x = 5;
	int y = 9;
	struct AtomicPair pair = { 3, &x };
	struct AtomicPair *pp = &pair;

	if (pair.value != 3)
		return 1;
	if (*pair.ptr != 5)
		return 2;

	pp->value = 7;
	if (pair.value != 7)
		return 3;

	pp->ptr = &y;
	if (*pair.ptr != 9)
		return 4;

	*pp->ptr = 13;
	if (y != 13)
		return 5;

	return 42;
}
