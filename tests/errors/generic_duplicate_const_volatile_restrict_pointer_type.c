int main(void)
{
	int x = 0;
	int *const volatile restrict p = &x;

	return _Generic(p, int *const volatile restrict: 1, int *: 2, default: 3);
}
