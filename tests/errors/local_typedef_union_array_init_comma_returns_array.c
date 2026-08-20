typedef union {
	int x;
} U;

void
f(void)
{
	U arr[1] = {{1}}, bad(void)[1];
}
