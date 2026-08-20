#include <stdatomic.h>

struct AtomicBox {
	atomic_int total;
	atomic_uint bits[2];
};

int
main(void)
{
	struct AtomicBox box = {
		ATOMIC_VAR_INIT(1),
		{ ATOMIC_VAR_INIT(2u), ATOMIC_VAR_INIT(4u) }
	};

	if (atomic_load(&box.total) != 1)
		return 1;
	if (atomic_load(&box.bits[0]) != 2u)
		return 2;

	atomic_store(&box.bits[0], 7u);
	if (atomic_fetch_add(&box.total, atomic_load(&box.bits[0])) != 1)
		return 3;
	if (atomic_load(&box.total) != 8)
		return 4;

	if (atomic_exchange(&box.bits[1], 9u) != 4u)
		return 5;
	if (atomic_load(&box.bits[1]) != 9u)
		return 6;

	return 42;
}
