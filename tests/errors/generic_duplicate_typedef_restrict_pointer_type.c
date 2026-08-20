typedef int *restrict rip_t;

int main(void)
{
	int x = 0;
	rip_t p = &x;

	return _Generic(p, rip_t: 1, int *: 2, default: 3);
}
