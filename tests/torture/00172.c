#include <stdio.h>

int main()
{
   int a;
   int b;
   int *d;
   int *e;
   d = &a;
   e = &b;
   a = 12;
   b = 34;
   fprintf(stderr,"%d\n", *d);
   fprintf(stderr,"%d\n", *e);
   fprintf(stderr,"%d\n", d == e);
   fprintf(stderr,"%d\n", d != e);
   d = e;
   fprintf(stderr,"%d\n", d == e);
   fprintf(stderr,"%d\n", d != e);

   return 0;
}

/* vim: set expandtab ts=4 sw=3 sts=3 tw=80 :*/
