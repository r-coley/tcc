#include <stdatomic.h>

int
main(void)
{
	atomic_uint bits = ATOMIC_VAR_INIT(0x12u);
	atomic_flag flag = ATOMIC_FLAG_INIT;

	if (atomic_fetch_add_explicit(&bits, 3u, memory_order_relaxed) != 0x12u)
		return 1;
	if (atomic_fetch_sub(&bits, 1u) != 0x15u)
		return 2;
	if (atomic_fetch_or(&bits, 0x80u) != 0x14u)
		return 3;
	if (atomic_fetch_xor_explicit(&bits, 0x03u, memory_order_seq_cst) != 0x94u)
		return 4;
	if (atomic_fetch_and(&bits, 0x95u) != 0x97u)
		return 5;
	if (atomic_load(&bits) != 0x95u)
		return 6;

	if (atomic_flag_test_and_set(&flag) != 0)
		return 7;
	if (atomic_flag_test_and_set_explicit(&flag, memory_order_acquire) != 1)
		return 8;
	atomic_flag_clear_explicit(&flag, memory_order_release);
	if (atomic_flag_test_and_set(&flag) != 0)
		return 9;

	if (kill_dependency(41) != 41)
		return 10;

	return 42;
}
