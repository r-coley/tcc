int
main(void)
{
    goto inside;
    {
        struct S {
            int x;
        };
        typedef struct S S;
        union U {
            int x;
            long y;
        };
        typedef union U U;
        enum E {
            E_VALUE = 1
        };
        typedef enum E E;
inside:
        return 42;
    }
}
