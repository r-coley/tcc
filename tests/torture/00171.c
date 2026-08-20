#include <stdio.h>

int main()
{
   int a;
   int *b;
   int *c;

   a = 42;
   b = &a;
   c = NULL;

   fprintf(stderr,"%d\n", *b);

   if (b == NULL)
      fprintf(stderr,"b is NULL\n");
   else
      fprintf(stderr,"b is not NULL\n");

   if (c == NULL)
      fprintf(stderr,"c is NULL\n");
   else
      fprintf(stderr,"c is not NULL\n");

   return 0;
}

/* vim: set expandtab ts=4 sw=3 sts=3 tw=80 :*/
