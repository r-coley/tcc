int
main(void)
{
    goto inside;
    {
        struct S {
            int x;
        };
        union U {
            int x;
            long y;
        };
        enum E {
            E_VALUE = 1
        };
inside:
        return 42;
    }
}
