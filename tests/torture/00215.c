#include <stdio.h>

extern int fprintf(FILE *,const char *format, ...);
static void kb_wait_1(void)
{
    unsigned long timeout = 2;
    do {
        if (1) fprintf(stderr,"timeout=%ld\n", timeout);
        else
        {
            while (1)
                fprintf(stderr,"error\n");
        }
        timeout--;
    } while (timeout);
}
static void kb_wait_2(void)
{
    unsigned long timeout = 2;
    do {
        if (1) fprintf(stderr,"timeout=%ld\n", timeout);
        else
        {
            for (;;)
                fprintf(stderr,"error\n");
        }
        timeout--;
    } while (timeout);
}
static void kb_wait_2_1(void)
{
    unsigned long timeout = 2;
    do {
        if (1) fprintf(stderr,"timeout=%ld\n", timeout);
        else
        {
            do {
                fprintf(stderr,"error\n");
            } while (1);
        }
        timeout--;
    } while (timeout);
}
static void kb_wait_2_2(void)
{
    unsigned long timeout = 2;
    do {
        if (1) fprintf(stderr,"timeout=%ld\n", timeout);
        else
        {
            label:
                fprintf(stderr,"error\n");
            goto label;
        }
        timeout--;
    } while (timeout);
}
static void kb_wait_3(void)
{
    unsigned long timeout = 2;
    do {
        if (1) fprintf(stderr,"timeout=%ld\n", timeout);
        else
        {
            int i = 1;
            goto label;
            i = i + 2;
        label:
            i = i + 3;
        }
        timeout--;
    } while (timeout);
}
static void kb_wait_4(void)
{
    unsigned long timeout = 2;
    do {
	if (1) fprintf(stderr,"timeout=%ld\n", timeout);
        else
        {
            switch(timeout) {
                case 2:
                    fprintf(stderr,"timeout is 2");
                    break;
                case 1:
                    fprintf(stderr,"timeout is 1");
                    break;
                default:
                    fprintf(stderr,"timeout is 0?");
                    break;
            };
            /* return; */
        }
        timeout--;
    } while (timeout);
}
int main()
{
    fprintf(stderr,"begin\n");
    kb_wait_1();
    kb_wait_2();
    kb_wait_2_1();
    kb_wait_2_2();
    kb_wait_3();
    kb_wait_4();
    fprintf(stderr,"end\n");
    return 0;
}
