#include <stdatomic.h>

int
main(void)
{
	atomic_int value = ATOMIC_VAR_INIT(5);
	int expected;

#ifdef __STDC_NO_ATOMICS__
	return 1;
#endif
	if (ATOMIC_INT_LOCK_FREE != 2)
		return 2;
	if (atomic_load(&value) != 5)
		return 3;

	atomic_store(&value, 11);
	if (atomic_exchange(&value, 17) != 11)
		return 4;
	if (atomic_load_explicit(&value, memory_order_relaxed) != 17)
		return 5;

	expected = 17;
	if (!atomic_compare_exchange_strong(&value, &expected, 23))
		return 6;
	if (atomic_load(&value) != 23)
		return 7;

	expected = 19;
	if (atomic_compare_exchange_weak_explicit(&value, &expected, 29,
	                                          memory_order_acq_rel,
	                                          memory_order_acquire))
		return 8;
	if (expected != 23)
		return 9;

	atomic_thread_fence(memory_order_seq_cst);
	atomic_signal_fence(memory_order_release);
	if (!atomic_is_lock_free(&value))
		return 10;

	return 42;
}
