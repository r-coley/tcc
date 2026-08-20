union Value {
    int i;
    char c;
};

int main(void)
{
    union Value value;
    union Value *ptr = &value;
    ptr->i = 42;
    return ptr->i == 42 ? 0 : 1;
}
