#include <stdio.h>

int main()
{
   fprintf(stderr,"%d\n", '\1');
   fprintf(stderr,"%d\n", '\10');
   fprintf(stderr,"%d\n", '\100');
   fprintf(stderr,"%d\n", '\x01');
   fprintf(stderr,"%d\n", '\x0e');
   fprintf(stderr,"%d\n", '\x10');
   fprintf(stderr,"%d\n", '\x40');
   fprintf(stderr,"test \x40\n");

   return 0;
}

/* vim: set expandtab ts=4 sw=3 sts=3 tw=80 :*/
