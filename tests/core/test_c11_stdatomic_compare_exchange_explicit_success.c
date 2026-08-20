#include <stdatomic.h>

int
main(void)
{
	atomic_uint value = ATOMIC_VAR_INIT(12u);
	unsigned expected = 12u;

	if (!atomic_compare_exchange_strong_explicit(&value, &expected, 21u,
	                                             memory_order_acq_rel,
	                                             memory_order_acquire))
		return 1;
	if (expected != 12u)
		return 2;
	if (atomic_load_explicit(&value, memory_order_relaxed) != 21u)
		return 3;

	expected = 7u;
	if (atomic_compare_exchange_strong_explicit(&value, &expected, 30u,
	                                            memory_order_release,
	                                            memory_order_relaxed))
		return 4;
	if (expected != 21u)
		return 5;
	if (atomic_load(&value) != 21u)
		return 6;

	return 42;
}
