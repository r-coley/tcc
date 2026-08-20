#include <stdio.h>

int fprintf(FILE *,const char *, ...);
char t[] = "012345678";

int main(void)
{
    char *data = t;
    unsigned long long r = 4;
    unsigned a = 5;
    unsigned long long b = 12;

    *(unsigned*)(data + r) += a - b;

    fprintf(stderr,"data = \"%s\"\n", data);
    return 0;
}
