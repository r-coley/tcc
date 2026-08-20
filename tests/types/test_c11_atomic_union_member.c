union AtomicCell {
	_Atomic int value;
	_Atomic(int *) ptr;
};

int
main(void)
{
	int x = 5;
	int y = 9;
	union AtomicCell cell;
	union AtomicCell *cp = &cell;

	cp->value = 7;
	if (cell.value != 7)
		return 1;

	cp->ptr = &x;
	if (*cell.ptr != 5)
		return 2;

	cell.ptr = &y;
	if (*cp->ptr != 9)
		return 3;

	*cp->ptr = 13;
	if (y != 13)
		return 4;

	return 42;
}
