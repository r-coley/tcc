int main(void)
{
	int x = 0;
	int *volatile restrict p = &x;

	return _Generic(p, int *volatile restrict: 43, default: 1);
}
