#define HDR_NAME() "tests/preproc/test_include_macro_expansion.h"
#define HDR_WRAP() HDR_NAME()

#include HDR_WRAP()

int
main(void)
{
	return macro_include_value == 42 ? 0 : 1;
}
