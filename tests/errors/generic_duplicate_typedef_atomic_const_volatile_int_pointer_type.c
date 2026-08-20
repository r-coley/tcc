typedef _Atomic(const volatile int *) atomic_cvip_t;

int
main(void)
{
	int x = 0;
	atomic_cvip_t ptr = &x;

	return _Generic(ptr, atomic_cvip_t: 1, const volatile int *: 2, default: 3);
}
