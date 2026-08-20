#define FLAG 1
\
#include "tests/preproc/test_directive_line_splice.h"
\
#ifdef FLAG
int main(void)
{
	return TCC_SPLICE_HEADER_VALUE;
}
\
#else
int main(void)
{
	return 1;
}
\
#endif
