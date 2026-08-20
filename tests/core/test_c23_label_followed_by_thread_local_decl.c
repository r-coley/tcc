int
main(void)
{
label:
	thread_local int x = 1;
	return x;
}
