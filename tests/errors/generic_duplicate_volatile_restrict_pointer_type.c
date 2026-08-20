int main(void)
{
	int x = 0;
	int *volatile restrict p = &x;

	return _Generic(p, int *volatile restrict: 1, int *: 2, default: 3);
}
