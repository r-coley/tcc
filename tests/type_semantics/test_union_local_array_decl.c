union Value {
    int i;
    char c;
};

int main(void)
{
    union Value values[2];
    values[0].i = 7;
    values[1].i = 9;
    return values[0].i == 7 && values[1].i == 9 ? 0 : 1;
}
