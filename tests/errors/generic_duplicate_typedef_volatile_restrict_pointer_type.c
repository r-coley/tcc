typedef int *volatile restrict vrip_t;

int main(void)
{
	int x = 0;
	vrip_t p = &x;

	return _Generic(p, vrip_t: 1, int *: 2, default: 3);
}
