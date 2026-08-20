#define LOCAL_HDR_NAME() "include_macro_search_local.h"
#define LOCAL_HDR_WRAP() LOCAL_HDR_NAME()

#include LOCAL_HDR_WRAP()

int
main(void)
{
    return INCLUDE_MACRO_SEARCH_VALUE;
}
