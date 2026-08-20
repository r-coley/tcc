#include <stdatomic.h>

int
main(void)
{
	int x = 5;
	int y = 9;
	int z = 13;
	_Atomic(int *) ptr = ATOMIC_VAR_INIT(&x);
	_Atomic(int *) typed = ATOMIC_VAR_INIT(&x);
	int *expected;
	int *old;

	if (*atomic_load(&ptr) != 5)
		return 1;

	atomic_store(&ptr, &y);
	if (*atomic_load_explicit(&ptr, memory_order_relaxed) != 9)
		return 2;

	if (_Generic(atomic_exchange(&typed, &y), int *: 1, default: 0) != 1)
		return 3;
	old = atomic_exchange(&ptr, &z);
	if (old != &y)
		return 4;
	if (*atomic_load(&ptr) != 13)
		return 5;

	expected = &z;
	if (!atomic_compare_exchange_strong_explicit(&ptr, &expected, &x,
	                                             memory_order_acq_rel,
	                                             memory_order_acquire))
		return 6;
	if (expected != &z)
		return 7;
	if (*atomic_load(&ptr) != 5)
		return 8;

	expected = &y;
	if (atomic_compare_exchange_weak_explicit(&ptr, &expected, &z,
	                                          memory_order_release,
	                                          memory_order_relaxed))
		return 9;
	if (expected != &x)
		return 10;
	if (*atomic_load(&ptr) != 5)
		return 11;

	return 42;
}
