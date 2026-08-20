typedef _Atomic(int) atomic_int_t;

int main(void)
{
	atomic_int_t *ptr = 0;

	return _Generic(ptr, atomic_int_t *: 42, default: 1);
}
