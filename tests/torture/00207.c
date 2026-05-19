#include <stdio.h>

/* This test segfaults as of April 27, 2015. */
void f1(int argc)
{
  char test[argc];
  if(0)
  label:
    fprintf(stderr,"boom!\n");
  if(argc-- == 0)
    return;
  goto label;
}

/* This segfaulted on 2015-11-19. */
void f2(void)
{
    goto start;
    {
        int a[1 && 1]; /* not a variable-length array */
        int b[1 || 1]; /* not a variable-length array */
        int c[1 ? 1 : 1]; /* not a variable-length array */
    start:
        a[0] = 0;
        b[0] = 0;
        c[0] = 0;
    }
}

void f3(void)
{
    fprintf(stderr,"%d\n", 0 ? fprintf(stderr,"x1\n") : 11);
    fprintf(stderr,"%d\n", 1 ? 12 : fprintf(stderr,"x2\n"));
    fprintf(stderr,"%d\n", 0 && fprintf(stderr,"x3\n"));
    fprintf(stderr,"%d\n", 1 || fprintf(stderr,"x4\n"));
}

int main()
{
  f1(2);
  f2();
  f3();

  return 0;
}
