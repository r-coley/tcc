#include <stdio.h>

int main()
{
   char a;
   int b;
   double c;

   fprintf(stderr,"%d\n", sizeof(a));
   fprintf(stderr,"%d\n", sizeof(b));
   fprintf(stderr,"%d\n", sizeof(c));

   fprintf(stderr,"%d\n", sizeof(!a));

   return 0;
}

/* vim: set expandtab ts=4 sw=3 sts=3 tw=80 :*/
