#include <stdio.h>

struct ziggy
{
   int a;
   int b;
   int c;
} bolshevic;

int main()
{
   int a;
   int *b;
   int c;

   a = 42;
   b = &a;
   fprintf(stderr,"a = %d\n", *b);

   bolshevic.a = 12;
   bolshevic.b = 34;
   bolshevic.c = 56;

   fprintf(stderr,"bolshevic.a = %d\n", bolshevic.a);
   fprintf(stderr,"bolshevic.b = %d\n", bolshevic.b);
   fprintf(stderr,"bolshevic.c = %d\n", bolshevic.c);

   struct ziggy *tsar = &bolshevic;

   fprintf(stderr,"tsar->a = %d\n", tsar->a);
   fprintf(stderr,"tsar->b = %d\n", tsar->b);
   fprintf(stderr,"tsar->c = %d\n", tsar->c);

   b = &(bolshevic.b);
   fprintf(stderr,"bolshevic.b = %d\n", *b);

   return 0;
}

// vim: set expandtab ts=4 sw=3 sts=3 tw=80 :
