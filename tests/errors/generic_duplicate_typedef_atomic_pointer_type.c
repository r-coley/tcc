typedef _Atomic(int *) atomic_iptr_t;

int
main(void)
{
	int x = 0;
	atomic_iptr_t ptr = &x;

	return _Generic(ptr, atomic_iptr_t: 1, int *: 2, default: 3);
}
