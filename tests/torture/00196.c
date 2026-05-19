#include <stdio.h>

int fred()
{
   fprintf(stderr,"fred\n");
   return 0;
}

int joe()
{
   fprintf(stderr,"joe\n");
   return 1;
}

int main()
{
   fprintf(stderr,"%d\n", fred() && joe());
   fprintf(stderr,"%d\n", fred() || joe());
   fprintf(stderr,"%d\n", joe() && fred());
   fprintf(stderr,"%d\n", joe() || fred());
   fprintf(stderr,"%d\n", fred() && (1 + joe()));
   fprintf(stderr,"%d\n", fred() || (0 + joe()));
   fprintf(stderr,"%d\n", joe() && (0 + fred()));
   fprintf(stderr,"%d\n", joe() || (1 + fred()));

   return 0;
}

/* vim: set expandtab ts=4 sw=3 sts=3 tw=80 :*/
