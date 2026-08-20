typedef _Atomic(const int *) atomic_cip_t;

int main(void)
{
	int x = 0;
	atomic_cip_t ptr = &x;

	return _Generic(ptr, atomic_cip_t: 1, const int *: 2, default: 3);
}
