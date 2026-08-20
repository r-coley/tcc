typedef _Atomic(int) atomic_int_t;

int main(void)
{
	atomic_int_t value = 0;

	return _Generic(value, atomic_int_t: 1, int: 2, default: 3);
}
