int main(void)
{
	int x = 0;
	_Atomic(const int *) ptr = &x;

	return _Generic(ptr, _Atomic(const int *): 1, const int *: 2, default: 3);
}
