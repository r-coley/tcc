int main(void)
{
	return _Generic(1L, int: 1, default: 2, default: 3);
}
