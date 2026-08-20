#ifdef __STDC_NO_ATOMICS__
#error __STDC_NO_ATOMICS__ should not be defined
#endif

#if __STDC_NO_THREADS__ != 1
#error __STDC_NO_THREADS__ should be defined as 1
#endif

int
main(void)
{
	return 0;
}
