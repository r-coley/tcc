char *__builtin_stack_save(void);

int
main(void)
{
    int i;
    int n = 4;
    char *before = __builtin_stack_save();

    for (i = 0; i < 3; i = i + 1) {
        int values[n];
        values[0] = i;
        continue;
    }

    return __builtin_stack_save() == before ? 42 : 1;
}
