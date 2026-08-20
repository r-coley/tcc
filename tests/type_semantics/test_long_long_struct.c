/* long long in structs */
struct S {
    char a;
    long long b;
    char c;
};

int main(void)
{
    if (sizeof(long long) != 8) return 1;
    if (sizeof(struct S) != 24) return 2;  /* 1 + 7pad + 8 + 1 + 7pad */

    struct S s;
    s.b = 0x123456789ABCDEF0LL;
    if (s.b != 0x123456789ABCDEF0LL) return 3;

    return 42;
}
