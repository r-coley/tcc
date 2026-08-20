#include <stdio.h>

void fred()
{
   fprintf(stderr,"In fred()\n");
   goto done;
   fprintf(stderr,"In middle\n");
done:
   fprintf(stderr,"At end\n");
}

void joe()
{
   int b = 5678;

   fprintf(stderr,"In joe()\n");

   {
      int c = 1234;
      fprintf(stderr,"c = %d\n", c);
      goto outer;
      fprintf(stderr,"uh-oh\n");
   }

outer:    

   fprintf(stderr,"done\n");
}

void henry()
{
   int a;

   fprintf(stderr,"In henry()\n");
   goto inner;

   {
      int b;
inner:    
      b = 1234;
      fprintf(stderr,"b = %d\n", b);
   }

   fprintf(stderr,"done\n");
}

int main()
{
   fred();
   joe();
   henry();

   return 0;
}

/* vim: set expandtab ts=4 sw=3 sts=3 tw=80 :*/
