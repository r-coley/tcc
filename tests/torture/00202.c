#include <stdio.h>

#define P(A,B) A ## B ; bob
#define Q(A,B) A ## B+

int main(void)
{
    int bob, jim = 21;
    bob = P(jim,) *= 2;
    fprintf(stderr,"jim: %d, bob: %d\n", jim, bob);
    jim = 60 Q(+,)3;
    fprintf(stderr,"jim: %d\n", jim);
    return 0;
}
