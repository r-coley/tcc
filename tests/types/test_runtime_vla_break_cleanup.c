char *__builtin_stack_save(void);

int
main(void)
{
    int n = 4;
    char *before = __builtin_stack_save();

    while (1) {
        int values[n];
        values[0] = 7;
        break;
    }

    return __builtin_stack_save() == before ? 42 : 1;
}
