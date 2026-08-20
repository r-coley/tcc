#define LOCAL_HEADER "tests/preproc/test_include_macro_expansion.h"
#define LOCAL_HEADER_INDIRECT LOCAL_HEADER
# include LOCAL_HEADER_INDIRECT

#define SYSTEM_HEADER <stdbool.h>
#define SYSTEM_HEADER_INDIRECT SYSTEM_HEADER
# include SYSTEM_HEADER_INDIRECT

int
main(void)
{
	bool ok = true;
	return (macro_include_value == 42 && ok) ? 0 : 1;
}
