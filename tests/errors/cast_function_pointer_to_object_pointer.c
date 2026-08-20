int f(void)
{
	return 1;
}

int main(void)
{
	void *p = (void *)f;
	return p != 0;
}
