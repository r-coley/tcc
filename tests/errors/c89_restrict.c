int main(void)
{
	int x = 1;
	int * restrict p = &x;
	return *p;
}
