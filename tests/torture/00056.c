#include <stdio.h>

int main() 
{
   int a;
   a = 42;
   fprintf(stderr,"%d\n", a);

   int b = 64;
   fprintf(stderr,"%d\n", b);

   int c = 12, d = 34;
   fprintf(stderr,"%d, %d\n", c, d);

   return 0;
}

// vim: set expandtab ts=4 sw=3 sts=3 tw=80 :
