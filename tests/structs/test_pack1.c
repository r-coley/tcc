/* Test #pragma pack(1) - all fields tightly packed, no alignment padding */
#include <stddef.h>

#pragma pack(1)
struct Y {
    char   c;
    int    i;
    long   l;
    double d;
    char  *cptr;
    int   *iptr;
    long  *lptr;
    double *dptr;
};
#pragma pack()

typedef struct Y Ytype;

int main(void) {
    int fail = 0;

    /* struct Y - pack(1), no padding between fields */
    if (offsetof(struct Y, c)    != 0)  fail++;
    if (offsetof(struct Y, i)    != 1)  fail++;
    if (offsetof(struct Y, l)    != 5)  fail++;
    if (offsetof(struct Y, d)    != 13) fail++;
    if (offsetof(struct Y, cptr) != 21) fail++;
    if (offsetof(struct Y, iptr) != 29) fail++;
    if (offsetof(struct Y, lptr) != 37) fail++;
    if (offsetof(struct Y, dptr) != 45) fail++;

    /* Ytype - same layout via typedef */
    if (offsetof(Ytype, c)    != 0)  fail++;
    if (offsetof(Ytype, i)    != 1)  fail++;
    if (offsetof(Ytype, l)    != 5)  fail++;
    if (offsetof(Ytype, d)    != 13) fail++;
    if (offsetof(Ytype, cptr) != 21) fail++;
    if (offsetof(Ytype, iptr) != 29) fail++;
    if (offsetof(Ytype, lptr) != 37) fail++;
    if (offsetof(Ytype, dptr) != 45) fail++;

    return fail;
}
