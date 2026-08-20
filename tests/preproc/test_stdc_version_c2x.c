#ifndef __STDC_VERSION__
#error __STDC_VERSION__ should be defined for C2x/C23
#endif

#if __STDC_VERSION__ != 202311L
#error unexpected C2x/C23 __STDC_VERSION__ value
#endif

int
main(void)
{
	return 0;
}
