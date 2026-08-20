char *__builtin_stack_save(void);

int
main(void)
{
    char *before = __builtin_stack_save();

    {
        int n = 5;
        int values[n];
        values[0] = 42;
    }

    return __builtin_stack_save() == before ? 42 : 1;
}
