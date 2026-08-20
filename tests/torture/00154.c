#include <stdio.h>

struct fred
{
   int boris;
   int natasha;
};

int main()
{
   struct fred bloggs;

   bloggs.boris = 12;
   bloggs.natasha = 34;

   fprintf(stderr,"%d\n", bloggs.boris);
   fprintf(stderr,"%d\n", bloggs.natasha);

   struct fred jones[2];
   jones[0].boris = 12;
   jones[0].natasha = 34;
   jones[1].boris = 56;
   jones[1].natasha = 78;

   fprintf(stderr,"%d\n", jones[0].boris);
   fprintf(stderr,"%d\n", jones[0].natasha);
   fprintf(stderr,"%d\n", jones[1].boris);
   fprintf(stderr,"%d\n", jones[1].natasha);

   return 0;
}
