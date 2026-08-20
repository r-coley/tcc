#ifndef __STDC_VERSION__
#error __STDC_VERSION__ should be defined for C99
#endif

#if __STDC_VERSION__ != 199901L
#error unexpected C99 __STDC_VERSION__ value
#endif

int
main(void)
{
	return 0;
}
