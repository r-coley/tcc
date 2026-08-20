int main(void)
{
	int x = 0;
	int *restrict p = &x;

	return _Generic(p, int *restrict: 42, default: 1);
}
