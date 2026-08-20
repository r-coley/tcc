int main(void)
{
	_Atomic(int) value = 0;

	return _Generic(value, _Atomic(int): 42, default: 1);
}
