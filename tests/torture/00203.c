#include <stdio.h>

int main()
{
    long long int res = 0;

    if (res < -2147483648LL) {
        fprintf(stderr,"Error: 0 < -2147483648\n");
        return 1;
    }
    else
    if (2147483647LL < res) {
        fprintf(stderr,"Error: 2147483647 < 0\n");
        return 2;
    }
    else
        fprintf(stderr,"long long constant test ok.\n");
    return 0;
}
