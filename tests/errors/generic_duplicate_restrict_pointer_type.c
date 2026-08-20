int main(void)
{
	int x = 0;
	int *restrict p = &x;

	return _Generic(p, int *restrict: 1, int *: 2, default: 3);
}
