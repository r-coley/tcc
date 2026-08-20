#include <stdio.h>

enum fred { a, b, c };

int main()
{
   fprintf(stderr,"a=%d\n", a);
   fprintf(stderr,"b=%d\n", b);
   fprintf(stderr,"c=%d\n", c);

   enum fred d;

   typedef enum { e, f, g } h;
   typedef enum { i, j, k } m;

   fprintf(stderr,"e=%d\n", e);
   fprintf(stderr,"f=%d\n", f);
   fprintf(stderr,"g=%d\n", g);

   fprintf(stderr,"i=%d\n", i);
   fprintf(stderr,"j=%d\n", j);
   fprintf(stderr,"k=%d\n", k);

   return 0;
}

/* vim: set expandtab ts=4 sw=3 sts=3 tw=80 :*/
