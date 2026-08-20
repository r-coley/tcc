#include <stdio.h>

int main()
{
   fprintf(stderr,"#include test\n");

#if 1
#if 0
   fprintf(stderr,"a\n");
#else
   fprintf(stderr,"b\n");
#endif
#else
#if 0
   fprintf(stderr,"c\n");
#else
   fprintf(stderr,"d\n");
#endif
#endif

#if 0
#if 1
   fprintf(stderr,"e\n");
#else
   fprintf(stderr,"f\n");
#endif
#else
#if 1
   fprintf(stderr,"g\n");
#else
   fprintf(stderr,"h\n");
#endif
#endif

#define DEF

#ifdef DEF
#ifdef DEF
   fprintf(stderr,"i\n");
#else
   fprintf(stderr,"j\n");
#endif
#else
#ifdef DEF
   fprintf(stderr,"k\n");
#else
   fprintf(stderr,"l\n");
#endif
#endif

#ifndef DEF
#ifndef DEF
   fprintf(stderr,"m\n");
#else
   fprintf(stderr,"n\n");
#endif
#else
#ifndef DEF
   fprintf(stderr,"o\n");
#else
   fprintf(stderr,"p\n");
#endif
#endif

#define ONE 1
#define ZERO 0

#if ONE
#if ZERO
   fprintf(stderr,"q\n");
#else
   fprintf(stderr,"r\n");
#endif
#else
#if ZERO
   fprintf(stderr,"s\n");
#else
   fprintf(stderr,"t\n");
#endif
#endif

   return 0;
}

/* vim: set expandtab ts=4 sw=3 sts=3 tw=80 :*/
