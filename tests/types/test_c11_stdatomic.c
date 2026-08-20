#include <stdatomic.h>

int
main(void)
{
	atomic_int value = ATOMIC_VAR_INIT(1);
	atomic_int expected = 3;
	atomic_flag flag = ATOMIC_FLAG_INIT;
	int old = 0;

	if (atomic_load(&value) != 1)
		return 1;
	atomic_init(&value, 3);
	if (atomic_load_explicit(&value, memory_order_seq_cst) != 3)
		return 2;
	atomic_store(&value, 5);
	if (atomic_load(&value) != 5)
		return 3;
	atomic_store_explicit(&value, 3, memory_order_release);
	if (!atomic_compare_exchange_strong(&value, &expected, 7))
		return 4;
	if (expected != 3)
		return 5;
	if (atomic_load(&value) != 7)
		return 6;
	expected = 1;
	if (atomic_compare_exchange_weak_explicit(&value, &expected, 9,
	                                          memory_order_acq_rel,
	                                          memory_order_acquire))
		return 7;
	if (expected != 7)
		return 8;
	atomic_thread_fence(memory_order_seq_cst);
	atomic_signal_fence(memory_order_relaxed);
	if (!atomic_is_lock_free(&value))
		return 9;
	old = atomic_exchange(&value, 11);
	if (old != 7 || atomic_load(&value) != 11)
		return 10;
	old = atomic_exchange_explicit(&value, 13, memory_order_acq_rel);
	if (old != 11 || atomic_load(&value) != 13)
		return 11;
	old = atomic_fetch_add(&value, 5);
	if (old != 13 || atomic_load(&value) != 18)
		return 12;
	old = atomic_fetch_sub_explicit(&value, 4, memory_order_acq_rel);
	if (old != 18 || atomic_load(&value) != 14)
		return 13;
	if (atomic_flag_test_and_set(&flag) != 0)
		return 14;
	if (atomic_flag_test_and_set_explicit(&flag, memory_order_acquire) != 1)
		return 15;
	atomic_flag_clear(&flag);
	if (atomic_flag_test_and_set(&flag) != 0)
		return 16;
	atomic_flag_clear_explicit(&flag, memory_order_release);

	return 42;
}
