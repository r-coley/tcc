#include <stdio.h>

int main()
{
   int a = 1;

   if (a)
      fprintf(stderr,"a is true\n");
   else
      fprintf(stderr,"a is false\n");

   int b = 0;
   if (b)
      fprintf(stderr,"b is true\n");
   else
      fprintf(stderr,"b is false\n");

   return 0;
}

// vim: set expandtab ts=4 sw=3 sts=3 tw=80 :
