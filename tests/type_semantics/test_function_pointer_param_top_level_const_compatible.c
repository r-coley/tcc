int id(int *p)
{
	return *p;
}

int main(void)
{
	int (*fp_const)(int *const) = id;
	int (*fp_plain)(int *) = fp_const;
	int x = 42;
	return fp_plain(&x) != 42;
}
