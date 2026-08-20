/* Regression coverage for identifier spelling across typedef-name lookup,
   macro expansion, and struct member access.  Earlier lexer-state experiments
   produced stage1-only token text corruption around similar inputs. */
#define FIELD value
#define PICK_TYPE(T) T

typedef struct Macro {
    int value;
    struct Macro *next;
} Macro;

static Macro table[2];

static int
read_macro(Macro *m)
{
    PICK_TYPE(Macro) *p = m;
    return p->FIELD;
}

int
main(void)
{
    table[0].value = 40;
    table[1].value = 2;
    table[0].next = &table[1];
    return read_macro(&table[0]) + read_macro(table[0].next);
}
