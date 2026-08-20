#include <stdio.h>

int main() 
{
   int Count;

   for (Count = 1; Count <= 10; Count++)
   {
      fprintf(stderr,"%d\n", Count);
   }

   return 0;
}

// vim: set expandtab ts=4 sw=3 sts=3 tw=80 :
