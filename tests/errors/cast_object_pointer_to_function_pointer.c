int main(void)
{
	void *p = 0;
	int (*fp)(void) = (int (*)(void))p;
	return fp != 0;
}
