int main(void)
{
	int x = 1;
	int a = _Alignof(++x);
	int b = _Alignof(x = 99);

	return x == 1 && a >= 1 && b >= 1 ? 42 : x;
}
