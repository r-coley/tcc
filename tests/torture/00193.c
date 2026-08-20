#include <stdio.h>

void fred(int x)
{
   switch (x)
   {
      case 1: fprintf(stderr,"1\n"); return;
      case 2: fprintf(stderr,"2\n"); break;
      case 3: fprintf(stderr,"3\n"); return;
   }

   fprintf(stderr,"out\n");
}

int main()
{
   fred(1);
   fred(2);
   fred(3);

   return 0;
}    

/* vim: set expandtab ts=4 sw=3 sts=3 tw=80 :*/
