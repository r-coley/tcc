int f(int *restrict *p);
int f(int **p) { return **p; }

int main(void)
{
	int x = 9;
	int *q = &x;
	return f(&q) - 9;
}
