/* Regression coverage for token text/source-span preservation used by
   macro stringification.  This is intentionally focused on spelling
   stability before retrying any broad LexerState refactor. */
#define STR(x) #x
#define WRAP(x) STR(x)
#define JOIN_STR(a, b) STR(a) STR(b)

static int
streq(const char *a, const char *b)
{
    while (*a && *b) {
        if (*a != *b)
            return 0;
        a++;
        b++;
    }
    return *a == *b;
}

int
main(void)
{
    const char *a = STR(alpha+beta_123);
    const char *b = STR((one,two,three));
    const char *c = WRAP(ID(foo));
    const char *d = JOIN_STR(left_token,right_token);

    if (!streq(a, "alpha+beta_123"))
        return 1;
    if (!streq(b, "(one,two,three)"))
        return 2;
    if (!streq(c, "ID(foo)"))
        return 3;
    if (!streq(d, "left_tokenright_token"))
        return 4;
    return 42;
}
