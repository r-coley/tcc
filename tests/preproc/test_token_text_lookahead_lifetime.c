/*
 * Token text / lookahead lifetime regression test.
 *
 * This protects spelling that flows through macro expansion, typedef-name
 * lookahead, struct member parsing, and macro-expanded field names.
 */

typedef struct Thing {
    int alpha;
    int beta;
} Thing;

#define TYPE_NAME Thing
#define FIELD_ONE alpha
#define FIELD_TWO beta

#define MAKE_VAR(name) name
#define PICK_FIELD(obj, field) ((obj).field)

static int
check_value(TYPE_NAME value)
{
    return PICK_FIELD(value, FIELD_ONE) + PICK_FIELD(value, FIELD_TWO);
}

int
main(void)
{
    TYPE_NAME MAKE_VAR(item);

    item.alpha = 17;
    item.beta = 25;

    if (check_value(item) != 42)
        return 1;

    return 42;
}
