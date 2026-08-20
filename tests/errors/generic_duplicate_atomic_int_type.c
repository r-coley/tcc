int main(void)
{
	_Atomic(int) value = 0;

	return _Generic(value, _Atomic(int): 1, int: 2, default: 3);
}
