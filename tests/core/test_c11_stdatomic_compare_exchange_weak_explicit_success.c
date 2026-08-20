#include <stdatomic.h>

int
main(void)
{
	atomic_uint value = ATOMIC_VAR_INIT(4u);
	unsigned expected = 4u;

	if (!atomic_compare_exchange_weak_explicit(&value, &expected, 10u,
	                                           memory_order_acq_rel,
	                                           memory_order_acquire))
		return 1;
	if (expected != 4u)
		return 2;
	if (atomic_load_explicit(&value, memory_order_relaxed) != 10u)
		return 3;

	expected = 1u;
	if (atomic_compare_exchange_weak_explicit(&value, &expected, 22u,
	                                          memory_order_release,
	                                          memory_order_relaxed))
		return 4;
	if (expected != 10u)
		return 5;
	if (atomic_load(&value) != 10u)
		return 6;

	return 42;
}
