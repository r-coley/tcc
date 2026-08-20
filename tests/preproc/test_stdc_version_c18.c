#ifndef __STDC_VERSION__
#error __STDC_VERSION__ should be defined for C18
#endif

#if __STDC_VERSION__ != 201710L
#error unexpected C18 __STDC_VERSION__ value
#endif

int
main(void)
{
	return 0;
}
