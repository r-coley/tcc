typedef int caf\u00E9_t;

struct menu_item {
    int jalape\u00F1o;
};

enum {
    pi\u00F1ata_value = 40
};

static caf\u00E9_t
add_\u03B1(caf\u00E9_t \u03B2)
{
    struct menu_item ni\u00F1o;

    ni\u00F1o.jalape\u00F1o = \u03B2;
    return ni\u00F1o.jalape\u00F1o + 2;
}

int
main(void)
{
    caf\u00E9_t r\u00E9sult = add_\u03B1(pi\u00F1ata_value);
    return r\u00E9sult;
}
