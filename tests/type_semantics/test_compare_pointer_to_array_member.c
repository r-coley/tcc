typedef struct S {
    int *p;
    int a[2];
} S;

int main(void)
{
    S s;

    s.p = s.a;
    return s.p == s.a ? 42 : 0;
}
