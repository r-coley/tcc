/*
 * Token text / macro path regression test.
 *
 * Covers __FILE__ / __LINE__ through macro expansion and stringification
 * of a macro argument whose spelling should survive token saving.
 */

#define LINE_VALUE() __LINE__
#define FILE_VALUE() __FILE__
#define STR1(x) #x
#define SPELL_ARG(x) STR1(x)

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

static int
contains(const char *s, const char *needle)
{
    int i = 0;
    int j;

    while (s[i]) {
        j = 0;
        while (needle[j] && s[i + j] == needle[j])
            j++;
        if (!needle[j])
            return 1;
        i++;
    }
    return 0;
}

int
main(void)
{
    int line = LINE_VALUE();
    const char *file = FILE_VALUE();
    const char *spelled = SPELL_ARG(alpha+beta(3));

    if (line <= 0)
        return 1;
    if (!contains(file, "test_token_text_line_macro_path.c"))
        return 2;
    if (!streq(spelled, "alpha+beta(3)"))
        return 3;

    return 42;
}
