typedef _Atomic(volatile int *) atomic_vip_t;

int
main(void)
{
	int x = 0;
	atomic_vip_t ptr = &x;

	return _Generic(ptr, atomic_vip_t: 42, default: 1);
}
