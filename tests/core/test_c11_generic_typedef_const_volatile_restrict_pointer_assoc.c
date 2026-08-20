typedef int *const volatile restrict cvrip_t;

int main(void)
{
	int x = 0;
	cvrip_t p = &x;

	return _Generic(p, cvrip_t: 42, default: 1);
}
