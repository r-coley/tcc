#ifndef __STDC_VERSION__
#error __STDC_VERSION__ should be defined for C11
#endif

#if __STDC_VERSION__ != 201112L
#error unexpected C11 __STDC_VERSION__ value
#endif

int
main(void)
{
	return 0;
}
