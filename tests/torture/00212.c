#include <stdio.h>

int
main()
{
#if defined(__LLP64__)
	if (sizeof(short) == 2
	    && sizeof(int) == 4
	    && sizeof(long int) == 4
	    && sizeof(long long int) == 8
	    && sizeof(void*) == 8) {
		(void)fprintf(stderr,"Ok\n");
	} else {
		(void)fprintf(stderr,"KO __LLP64__\n");
	}
#elif defined(__LP64__)
	if (sizeof(short) == 2
	    && sizeof(int) == 4
	    && sizeof(long int) == 8
	    && sizeof(long long int) == 8
	    && sizeof(void*) == 8) {
		(void)fprintf(stderr,"Ok\n");
	} else {
		(void)fprintf(stderr,"KO __LP64__\n");
	}
#elif defined(__ILP32__)
	if (sizeof(short) == 2
	    && sizeof(int) == 4
	    && sizeof(long int) == 4
	    && sizeof(void*) == 4) {
		(void)fprintf(stderr,"Ok\n");
	} else {
		(void)fprintf(stderr,"KO __ILP32__\n");
	}
#else
	(void)fprintf(stderr,"KO no __*LP*__ defined.\n");
#endif
}
