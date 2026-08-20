typedef int *volatile restrict vrip_t;

int main(void)
{
	int x = 0;
	vrip_t p = &x;

	return _Generic(p, vrip_t: 41, default: 1);
}
