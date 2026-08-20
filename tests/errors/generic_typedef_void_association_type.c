typedef void v;

int main(void)
{
	int x = 0;
	return _Generic(x, v: 1, int: 42, default: 3);
}
