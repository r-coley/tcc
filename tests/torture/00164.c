#include <stdio.h>

int main()
{
   int a;
   int b;
   int c;
   int d;
   int e;
   int f;
   int x;
   int y;

   a = 12;
   b = 34;
   c = 56;
   d = 78;
   e = 0;
   f = 1;

   fprintf(stderr,"%d\n", c + d);
   fprintf(stderr,"%d\n", (y = c + d));
   fprintf(stderr,"%d\n", e || e && f);
   fprintf(stderr,"%d\n", e || f && f);
   fprintf(stderr,"%d\n", e && e || f);
   fprintf(stderr,"%d\n", e && f || f);
   fprintf(stderr,"%d\n", a && f | f);
   fprintf(stderr,"%d\n", a | b ^ c & d);
   fprintf(stderr,"%d, %d\n", a == a, a == b);
   fprintf(stderr,"%d, %d\n", a != a, a != b);
   fprintf(stderr,"%d\n", a != b && c != d);
   fprintf(stderr,"%d\n", a + b * c / f);
   fprintf(stderr,"%d\n", a + b * c / f);
   fprintf(stderr,"%d\n", (4 << 4));
   fprintf(stderr,"%d\n", (64 >> 4));

   return 0;
}

// vim: set expandtab ts=4 sw=3 sts=3 tw=80 :
