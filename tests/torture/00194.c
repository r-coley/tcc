#include <stdio.h>

int main()
{
   int a;
   char b;

   a = 0;
   while (a < 2)
   {
      fprintf(stderr,"%d", a++);
      break;

      b = 'A';
      while (b < 'C')
      {
         fprintf(stderr,"%c", b++);
      }
      fprintf(stderr,"e");
   }
   fprintf(stderr,"\n");

   return 0;
}

/* vim: set expandtab ts=4 sw=3 sts=3 tw=80 :*/
