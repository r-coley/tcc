typedef struct caf\u00E9_tag {
    int val\u00E9;
} caf\u00E9_t;

enum {
    enum\u00E9 = 40
};

static int
add\u00E9(caf\u00E9_t arg, int param\u00E9)
{
    goto lab\u00E9l;
lab\u00E9l:
    return arg.val\u00E9 + param\u00E9 + enum\u00E9;
}

int
main(void)
{
    caf\u00E9_t obj = { 2 };
    return add\u00E9(obj, 0);
}
