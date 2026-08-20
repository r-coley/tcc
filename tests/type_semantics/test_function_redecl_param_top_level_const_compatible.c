int f(int *const p);
int f(int *p)
{
	return *p;
}

int main(void)
{
	int x = 42;
	return f(&x) != 42;
}
