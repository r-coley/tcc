#include <stdio.h>

int main() 
{
   fprintf(stderr,"Hello world\n");

   int Count;
   for (Count = -5; Count <= 5; Count++)
      fprintf(stderr,"Count = %d\n", Count);

   fprintf(stderr,"String 'hello', 'there' is '%s', '%s'\n", "hello", "there");
   fprintf(stderr,"Character 'A' is '%c'\n", 65);
   fprintf(stderr,"Character 'a' is '%c'\n", 'a');

   return 0;
}

// vim: set expandtab ts=4 sw=3 sts=3 tw=80 :
