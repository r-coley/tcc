#include <stdio.h>
#include <string.h>

int main()
{
   char a[10];

   strcpy(a, "hello");
   fprintf(stderr,"%s\n", a);

   strncpy(a, "gosh", 2);
   fprintf(stderr,"%s\n", a);

   fprintf(stderr,"%d\n", strcmp(a, "apple") > 0);
   fprintf(stderr,"%d\n", strcmp(a, "goere") > 0);
   fprintf(stderr,"%d\n", strcmp(a, "zebra") < 0);

   fprintf(stderr,"%d\n", strlen(a));

   strcat(a, "!");
   fprintf(stderr,"%s\n", a);

   fprintf(stderr,"%d\n", strncmp(a, "apple", 2) > 0);
   fprintf(stderr,"%d\n", strncmp(a, "goere", 2) == 0);
   fprintf(stderr,"%d\n", strncmp(a, "goerg", 2) == 0);
   fprintf(stderr,"%d\n", strncmp(a, "zebra", 2) < 0);

   fprintf(stderr,"%s\n", strchr(a, 'o'));
   fprintf(stderr,"%s\n", strrchr(a, 'l'));
   fprintf(stderr,"%d\n", strrchr(a, 'x') == NULL);

   memset(&a[1], 'r', 4);
   fprintf(stderr,"%s\n", a);

   memcpy(&a[2], a, 2);
   fprintf(stderr,"%s\n", a);

   fprintf(stderr,"%d\n", memcmp(a, "apple", 4) > 0);
   fprintf(stderr,"%d\n", memcmp(a, "grgr", 4) == 0);
   fprintf(stderr,"%d\n", memcmp(a, "zebra", 4) < 0);

   return 0;
}

/* vim: set expandtab ts=4 sw=3 sts=3 tw=80 :*/
