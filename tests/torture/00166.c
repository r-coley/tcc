#include <stdio.h>

int main()
{
   int a = 24680;
   int b = 01234567;
   int c = 0x2468ac;
   int d = 0x2468AC;

   fprintf(stderr,"%d\n", a);
   fprintf(stderr,"%d\n", b);
   fprintf(stderr,"%d\n", c);
   fprintf(stderr,"%d\n", d);

   return 0;
}

// vim: set expandtab ts=4 sw=3 sts=3 tw=80 :
