int f(const int *p);
int f(int *p) { return *p; }

int main(void)
{
	int x = 1;
	return f(&x) - 1;
}
