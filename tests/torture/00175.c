#include <stdio.h>

void charfunc(char a)
{
   fprintf(stderr,"char: %c\n", a);
}

void intfunc(int a)
{
   fprintf(stderr,"int: %d\n", a);
}

void floatfunc(float a)
{
   fprintf(stderr,"float: %f\n", a);
}

int main()
{
   charfunc('a');
   charfunc(98);
   charfunc(99.0);

   intfunc('a');
   intfunc(98);
   intfunc(99.0);

   floatfunc('a');
   floatfunc(98);
   floatfunc(99.0);

   /* fprintf(stderr,"%c %d %f\n", 'a', 'b', 'c'); */
   /* fprintf(stderr,"%c %d %f\n", 97, 98, 99); */
   /* fprintf(stderr,"%c %d %f\n", 97.0, 98.0, 99.0); */

   char b = 97;
   char c = 97.0;

   fprintf(stderr,"%d %d\n", b, c);

   int d = 'a';
   int e = 97.0;

   fprintf(stderr,"%d %d\n", d, e);

   float f = 'a';
   float g = 97;

   fprintf(stderr,"%f %f\n", f, g);

   return 0;
}

/* vim: set expandtab ts=4 sw=3 sts=3 tw=80 :*/
