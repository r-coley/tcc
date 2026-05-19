#include <stdio.h>

int main()
{
   FILE *f = fopen("/tmp/fred.txt", "w");
   fwrite("hello\nhello\n", 1, 12, f);
   fclose(f);

   char freddy[7];
   f = fopen("/tmp/fred.txt", "r");
   if (fread(freddy, 1, 6, f) != 6)
      fprintf(stderr,"couldn't read /tmp/fred.txt\n");

   freddy[6] = '\0';
   fclose(f);

   fprintf(stderr,"%s", freddy);

   int InChar;
   char ShowChar;
   f = fopen("/tmp/fred.txt", "r");
   while ( (InChar = fgetc(f)) != EOF)
   {
      ShowChar = InChar;
      if (ShowChar < ' ')
         ShowChar = '.';

      fprintf(stderr,"ch: %d '%c'\n", InChar, ShowChar);
   }
   fclose(f);

   f = fopen("/tmp/fred.txt", "r");
   while ( (InChar = getc(f)) != EOF)
   {
      ShowChar = InChar;
      if (ShowChar < ' ')
         ShowChar = '.';

      fprintf(stderr,"ch: %d '%c'\n", InChar, ShowChar);
   }
   fclose(f);

   f = fopen("/tmp/fred.txt", "r");
   while (fgets(freddy, sizeof(freddy), f) != NULL)
      fprintf(stderr,"x: %s", freddy);

   fclose(f);

   return 0;
}

/* vim: set expandtab ts=4 sw=3 sts=3 tw=80 :*/
