int main(void)
{
	int x = 0;
	return _Generic(x, void: 1, int: 42, default: 3);
}
