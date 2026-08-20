char *__builtin_stack_save(void);

int
main(void)
{
    int n = 4;
    char *before = __builtin_stack_save();

    {
        int values[n];
        values[0] = 7;
        goto done;
    }

done:
    return __builtin_stack_save() == before ? 42 : 1;
}
