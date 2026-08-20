int main(void)
{
	int x = 0;
	_Atomic(const int *) ptr = &x;

	return _Generic(ptr, _Atomic(const int *): 42, default: 1);
}
