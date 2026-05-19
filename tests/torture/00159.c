#include <stdio.h>

int myfunc(int x)
{
   return x * x;
}

void vfunc(int a)
{
   fprintf(stderr,"a=%d\n", a);
}

void qfunc()
{
   fprintf(stderr,"qfunc()\n");
}

void zfunc()
{
   ((void (*)(void))0) ();
}

int main()
{
   fprintf(stderr,"%d\n", myfunc(3));
   fprintf(stderr,"%d\n", myfunc(4));

   vfunc(1234);

   qfunc();

   return 0;
}

// vim: set expandtab ts=4 sw=3 sts=3 tw=80 :
