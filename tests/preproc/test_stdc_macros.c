#if __STDC__ != 1
#error __STDC__ should be 1
#endif

#if __STDC_HOSTED__ != 0
#error __STDC_HOSTED__ should be 0 for this freestanding compiler
#endif

#if __STDC_VERSION__ < 199901L
#error __STDC_VERSION__ should be defined for C99 and later
#endif

int
main(void)
{
	return 0;
}
