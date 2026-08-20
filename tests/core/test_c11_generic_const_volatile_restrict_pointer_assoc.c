int main(void)
{
	int x = 0;
	int *const volatile restrict p = &x;

	return _Generic(p, int *const volatile restrict: 44, default: 1);
}
