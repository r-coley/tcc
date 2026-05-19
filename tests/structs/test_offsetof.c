/* Test struct layout with double/float/long fields and offsetof */
#include <stddef.h>

struct X {
    char   c;
    int    i;
    long   l;
    double d;
    char  *cptr;
    int   *iptr;
    long  *lptr;
    double *dptr;
};

typedef struct X Xtype;

int main(void) {
    int fail = 0;

    /* struct X - natural alignment */
    if (offsetof(struct X, c)    != 0)  fail++;
    if (offsetof(struct X, i)    != 4)  fail++;
    if (offsetof(struct X, l)    != 8)  fail++;
    if (offsetof(struct X, d)    != 16) fail++;
    if (offsetof(struct X, cptr) != 24) fail++;
    if (offsetof(struct X, iptr) != 32) fail++;
    if (offsetof(struct X, lptr) != 40) fail++;
    if (offsetof(struct X, dptr) != 48) fail++;

    /* Xtype - same layout via typedef */
    if (offsetof(Xtype, c)    != 0)  fail++;
    if (offsetof(Xtype, i)    != 4)  fail++;
    if (offsetof(Xtype, l)    != 8)  fail++;
    if (offsetof(Xtype, d)    != 16) fail++;
    if (offsetof(Xtype, cptr) != 24) fail++;
    if (offsetof(Xtype, iptr) != 32) fail++;
    if (offsetof(Xtype, lptr) != 40) fail++;
    if (offsetof(Xtype, dptr) != 48) fail++;

    return fail;
}
