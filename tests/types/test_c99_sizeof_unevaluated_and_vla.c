static int ordinary_sizeof_is_unevaluated(void)
{
	int x = 1;
	int n = sizeof(++x);

	return x == 1 && n == (int)sizeof(int);
}

static int vla_sizeof_uses_runtime_bound(void)
{
	int n = 2;
	int values[++n];
	int bytes = sizeof(values);

	return n == 3 && bytes == 3 * (int)sizeof(int);
}

int main(void)
{
	return ordinary_sizeof_is_unevaluated() && vla_sizeof_uses_runtime_bound()
	       ? 42 : 1;
}
