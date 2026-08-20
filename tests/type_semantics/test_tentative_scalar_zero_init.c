int shared;
extern int shared;

int
main(void)
{
    if (shared != 0)
        return 1;
    shared = 42;
    return shared;
}
