#include <stdatomic.h>

union AtomicValue {
	atomic_int ivalue;
	atomic_uint uvalue;
};

int
main(void)
{
	union AtomicValue value = { ATOMIC_VAR_INIT(5) };
	union AtomicValue other = { ATOMIC_VAR_INIT(9u) };

	if (atomic_load(&value.ivalue) != 5)
		return 1;

	atomic_store(&value.ivalue, 11);
	if (atomic_exchange(&value.ivalue, 17) != 11)
		return 2;
	if (atomic_load_explicit(&value.ivalue, memory_order_relaxed) != 17)
		return 3;

	if (atomic_load(&other.uvalue) != 9u)
		return 4;
	if (atomic_fetch_add(&other.uvalue, 4u) != 9u)
		return 5;
	if (atomic_load(&other.uvalue) != 13u)
		return 6;

	return 42;
}
