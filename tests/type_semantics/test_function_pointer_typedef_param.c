typedef int (*op_t)(int);

int add2(int value)
{
    return value + 2;
}

int call_it(op_t fn, int value)
{
    return fn(value);
}

int main(void)
{
    return call_it(add2, 40);
}
