static int
f(int value)
{
	return value + 1;
}

int main(void)
{
	int (*fp)(int) = f;
	int (*gp)(int) = (int (*)(int))fp;

	return gp(41);
}
